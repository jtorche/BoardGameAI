#include "ML.h"

#ifdef USE_TINY_DNN

template<u32 SecondLayerSize>
struct TwoLayers : BaseNN
{
	TwoLayers(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		u32 tensorSize = bg::GameController::TensorSize + (useExtraTensorData ? bg::GameController::ExtraTensorSize : 0);

		m_net // << tiny_dnn::batch_normalization_layer(1, tensorSize)
			<< tiny_dnn::fully_connected_layer(tensorSize, SecondLayerSize)
			<< tiny_dnn::relu_layer()
			<< tiny_dnn::fully_connected_layer(SecondLayerSize, 1)
			<< tiny_dnn::sigmoid_layer();

		m_layer1Weights = nullptr;
		m_layer2Weights = nullptr;
		m_layer1Biases = nullptr;
		m_layer2Biases = nullptr;
		m_inputSize = tensorSize;
	}

	// Raw pointers into tiny-dnn internal buffers (set by prepareAfterLoad)
	float* m_layer1Weights = nullptr; // size: SecondLayerSize * inputSize
	float* m_layer2Weights = nullptr; // size: SecondLayerSize * 1
	float* m_layer1Biases = nullptr; // size: SecondLayerSize
	float* m_layer2Biases = nullptr; // size: 1
	u32    m_inputSize = 0;

	void prepareAfterLoad() override
	{
		// Network layout:
		// [0] fully_connected(input -> SecondLayerSize)
		// [1] relu
		// [2] fully_connected(SecondLayerSize -> 1)
		// [3] sigmoid
		using fc_layer = tiny_dnn::fully_connected_layer;

		// Layer 0: first fully-connected
		if (m_net.layer_size() > 0) {
			auto* l0 = dynamic_cast<fc_layer*>(m_net[0]);
			if (l0) {
				// tiny-dnn stores weights/biases in layer->weights():
				// weights()[0] = weight matrix, weights()[1] = bias vector (both as vec_t)
				auto& w0_ptr = *l0->weights()[0];
				auto& b0_ptr = *l0->weights()[1];

				tiny_dnn::vec_t& w0 = w0_ptr; // weight matrix
				tiny_dnn::vec_t& b0 = b0_ptr; // bias vector

				m_layer1Weights = w0.empty() ? nullptr : w0.data();
				m_layer1Biases = b0.empty() ? nullptr : b0.data();

				// Keep input size in sync (tiny-dnn exposes in_size/out_size)
				m_inputSize = static_cast<u32>(l0->in_size());
			}
		}

		// Layer 2: second fully-connected
		if (m_net.layer_size() > 2) {
			auto* l2 = dynamic_cast<fc_layer*>(m_net[2]);
			if (l2) {
				auto& w2_ptr = *l2->weights()[0];
				auto& b2_ptr = *l2->weights()[1];

				tiny_dnn::vec_t& w2 = w2_ptr;
				tiny_dnn::vec_t& b2 = b2_ptr;

				m_layer2Weights = w2.empty() ? nullptr : w2.data();
				m_layer2Biases = b2.empty() ? nullptr : b2.data();
			}
		}
	}

	// Manual, allocation-light forward pass using cached weight/bias pointers.
	tiny_dnn::vec_t forward(const tiny_dnn::vec_t& x, void* pThreadContext, u32 netAge) override
	{
		// Fallback to tiny-dnn if we don't have direct pointers (e.g., not prepared)
		if (!m_layer1Weights || !m_layer2Weights || !m_layer1Biases || !m_layer2Biases) {
			return BaseNN::forward(x, pThreadContext, netAge);
		}

		DEBUG_ASSERT(x.size() == m_inputSize);

		// First layer: y1 = relu(W1 * x + b1)
		// - W1: SecondLayerSize x inputSize, row-major
		// - b1: SecondLayerSize
		// Second layer: y2 = sigmoid(W2 * y1 + b2)
		// - W2: 1 x SecondLayerSize
		float hidden[SecondLayerSize];

		// L1: dense + ReLU
		for (u32 o = 0; o < SecondLayerSize; ++o) {
			float acc = m_layer1Biases[o];
			// dot product
			for (u32 i = 0; i < m_inputSize; ++i) {
				acc += m_layer1Weights[i * SecondLayerSize + o] * x[i];
			}
			// ReLU
			hidden[o] = acc > 0.0f ? acc : 0.0f;
		}

		// L2: dense + sigmoid, output size is 1
		float acc2 = m_layer2Biases[0];
		for (u32 i = 0; i < SecondLayerSize; ++i) {
			acc2 += m_layer2Weights[i] * hidden[i];
		}

		// Sigmoid
		float out = 1.0f / (1.0f + std::exp(-acc2));

		tiny_dnn::vec_t result(1);
		result[0] = out;

		//float test = BaseNN::forward(x, pThreadContext, netAge)[0];
		//DEBUG_ASSERT(std::abs(test - out) < 1e-5f);

		return result;
	}
};

#else

template<u32 SecondLayerSize>
struct TwoLayers : BaseNN
{
	torch::nn::Linear fully1 = nullptr;
	torch::nn::Linear fully2 = nullptr;

	TwoLayers(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		using namespace torch;
		u32 tensorSize = sevenWD::GameState::TensorSize +
			(useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

		fully1 = register_module("fully1", nn::Linear(tensorSize, SecondLayerSize));
		fully2 = register_module("fully2", nn::Linear(SecondLayerSize, 1));
	}

	torch::Tensor forward(torch::Tensor x) override {
		x = torch::relu(fully1->forward(x));
		return torch::sigmoid(fully2->forward(x));
	}
};

#endif

using TwoLayers64 = TwoLayers<64>;
using TwoLayers24 = TwoLayers<24>;
using TwoLayers8 = TwoLayers<8>;

#ifdef USE_TINY_DNN

struct BaseLine : BaseNN
{
	BaseLine(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		u32 tensorSize = bg::GameController::TensorSize +
			(useExtraTensorData ? bg::GameController::ExtraTensorSize : 0);

		m_net << tiny_dnn::fully_connected_layer(tensorSize, 1) << tiny_dnn::sigmoid_layer();
	}
};

template<u32 LayerSize, u32 NumLayers>
struct MultiLayersPUCT : BaseNN
{
	static_assert(NumLayers >= 2, "MultiLayersPUCT requires at least 2 layers");

	MultiLayersPUCT(NetworkType netType)
		: BaseNN(netType, true)
	{
		u32 tensorSize = bg::GameController::TensorSize + bg::GameController::ExtraTensorSize;
		constexpr u32 kOutSize = 1u + bg::GameController::cMaxNumMoves;

		m_net << tiny_dnn::batch_normalization_layer(1, tensorSize);

		// First FC: tensorSize -> LayerSize
		m_net << tiny_dnn::fully_connected_layer(tensorSize, LayerSize)
			  << tiny_dnn::relu_layer();

		// Intermediate FCs: LayerSize -> LayerSize
		for (u32 i = 1; i < NumLayers - 1; ++i) {
			m_net << tiny_dnn::fully_connected_layer(LayerSize, LayerSize)
				  << tiny_dnn::relu_layer();
		}

		// Final FC: LayerSize -> output
		m_net << tiny_dnn::fully_connected_layer(LayerSize, kOutSize)
			  << tiny_dnn::sigmoid_layer();

		m_bnMean = nullptr;
		m_bnVariance = nullptr;
		m_bnEpsilon = 0.0f;

		for (u32 i = 0; i < NumLayers; ++i) {
			m_layerWeights[i] = nullptr;
			m_layerBiases[i] = nullptr;
		}
		m_inputSize = tensorSize;
	}

	// Batch-norm (inference): y = (x - mean) / sqrt(variance + eps)
	float* m_bnMean = nullptr;     // size: inputSize
	float* m_bnVariance = nullptr; // size: inputSize
	float  m_bnEpsilon = 0.0f;

	// FC layers weights and biases
	// Layer 0: tensorSize -> LayerSize
	// Layers 1 to NumLayers-2: LayerSize -> LayerSize
	// Layer NumLayers-1: LayerSize -> (1 + cMaxNumMoves)
	float* m_layerWeights[NumLayers] = {};
	float* m_layerBiases[NumLayers] = {};

	u32 m_inputSize = 0;

	void prepareAfterLoad() override
	{
		using bn_layer = tiny_dnn::batch_normalization_layer;
		using fc_layer = tiny_dnn::fully_connected_layer;

		// Layout:
		// [0] batch_norm
		// [1] fully_connected(tensorSize -> LayerSize)
		// [2] relu
		// [3] fully_connected(LayerSize -> LayerSize)  (if NumLayers > 2)
		// [4] relu                                      (if NumLayers > 2)
		// ... repeat for intermediate layers
		// [2*NumLayers-1] fully_connected(LayerSize -> output)
		// [2*NumLayers] sigmoid

		// Layer 0: batch-norm
		if (m_net.layer_size() > 0) {
			auto* l0 = dynamic_cast<bn_layer*>(m_net[0]);
			if (l0) {
				auto& mean = l0->mean_;
				auto& variance = l0->variance_;

				m_bnMean = mean.empty() ? nullptr : mean.data();
				m_bnVariance = variance.empty() ? nullptr : variance.data();
				m_bnEpsilon = static_cast<float>(l0->epsilon());
			}
		}

		// FC layers: index in m_net is 1 + 2*i (skip batch_norm, then FC+relu pairs)
		for (u32 i = 0; i < NumLayers; ++i) {
			u32 netIndex = 1 + 2 * i;
			if (m_net.layer_size() > netIndex) {
				auto* fc = dynamic_cast<fc_layer*>(m_net[netIndex]);
				if (fc) {
					auto& w_ref = *fc->weights()[0];
					auto& b_ref = *fc->weights()[1];

					tiny_dnn::vec_t& w = w_ref;
					tiny_dnn::vec_t& b = b_ref;

					m_layerWeights[i] = w.empty() ? nullptr : w.data();
					m_layerBiases[i] = b.empty() ? nullptr : b.data();

					if (i == 0) {
						m_inputSize = static_cast<u32>(fc->in_size());
					}
				}
			}
		}
	}

	// Manual, allocation-light forward pass
	tiny_dnn::vec_t forward(const tiny_dnn::vec_t& x, void* pThreadContext, u32 netAge) override
	{
		// Check all pointers are valid
		bool valid = m_bnMean && m_bnVariance;
		for (u32 i = 0; i < NumLayers && valid; ++i) {
			valid = m_layerWeights[i] && m_layerBiases[i];
		}
		if (!valid) {
			return BaseNN::forward(x, pThreadContext, netAge);
		}

		DEBUG_ASSERT(x.size() == m_inputSize);

		constexpr u32 kOutSize = 1u + bg::GameController::cMaxNumMoves;

		// BN output: bn[i] = (x[i] - mean[i]) / sqrt(var[i] + eps)
		float bnOut[bg::GameController::TensorSize + bg::GameController::ExtraTensorSize];
		DEBUG_ASSERT(m_inputSize <= (sizeof(bnOut) / sizeof(bnOut[0])));

		for (u32 i = 0; i < m_inputSize; ++i) {
			const float denom = std::sqrt(m_bnVariance[i] + m_bnEpsilon);
			bnOut[i] = (x[i] - m_bnMean[i]) / denom;
		}

		// Use two buffers for ping-pong between layers
		float buffer1[LayerSize];
		float buffer2[LayerSize];
		float* input = bnOut;
		float* output = buffer1;
		u32 inputSize = m_inputSize;

		// Process all layers except the last one (with ReLU)
		for (u32 layer = 0; layer < NumLayers - 1; ++layer) {
			const float* weights = m_layerWeights[layer];
			const float* biases = m_layerBiases[layer];

			for (u32 o = 0; o < LayerSize; ++o) {
				float acc = biases[o];
				for (u32 i = 0; i < inputSize; ++i) {
					acc += weights[i * LayerSize + o] * input[i];
				}
				output[o] = acc > 0.0f ? acc : 0.0f; // ReLU
			}

			// Swap buffers for next layer
			input = output;
			output = (output == buffer1) ? buffer2 : buffer1;
			inputSize = LayerSize;
		}

		// Final layer (with sigmoid)
		tiny_dnn::vec_t result(kOutSize);
		const float* weights = m_layerWeights[NumLayers - 1];
		const float* biases = m_layerBiases[NumLayers - 1];

		for (u32 o = 0; o < kOutSize; ++o) {
			float acc = biases[o];
			for (u32 i = 0; i < LayerSize; ++i) {
				acc += weights[i * kOutSize + o] * input[i];
			}
			result[o] = 1.0f / (1.0f + std::exp(-acc)); // Sigmoid
		}

		return result;
	}
};

// Convenience aliases
template<u32 LayerSize>
using TwoLayersPUCT = MultiLayersPUCT<LayerSize, 2>;

template<u32 LayerSize>
using ThreeLayersPUCT = MultiLayersPUCT<LayerSize, 3>;

template<u32 LayerSize>
using FourLayersPUCT = MultiLayersPUCT<LayerSize, 4>;

#else

struct BaseLine : BaseNN
{
	torch::nn::Linear fully1 = nullptr;

	BaseLine(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		using namespace torch;
		u32 tensorSize = sevenWD::GameState::TensorSize +
			(useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

		fully1 = register_module("fully1", nn::Linear(tensorSize, 1));
	}

	torch::Tensor forward(torch::Tensor x) override {
		return torch::sigmoid(fully1->forward(x));
	}
};

#endif