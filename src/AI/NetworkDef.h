#include "ML.h"

#ifdef USE_TINY_DNN

template<u32 SecondLayerSize>
struct TwoLayers : BaseNN
{
	TwoLayers(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		u32 tensorSize = sevenWD::GameState::TensorSize +
			(useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

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

template<u32 SecondLayerSize>
struct TwoLayersPUCT : BaseNN
{
	TwoLayersPUCT(NetworkType netType)
		: BaseNN(netType, true)
	{
		u32 tensorSize = sevenWD::GameState::TensorSize + sevenWD::GameState::ExtraTensorSize;

		m_net << tiny_dnn::batch_normalization_layer(1, tensorSize)
			<< tiny_dnn::fully_connected_layer(tensorSize, SecondLayerSize)
			<< tiny_dnn::relu_layer()
			<< tiny_dnn::fully_connected_layer(SecondLayerSize, 1 + sevenWD::GameController::cMaxNumMoves)
			<< tiny_dnn::sigmoid_layer();

		m_bnMean = nullptr;
		m_bnVariance = nullptr;
		m_bnEpsilon = 0.0f;

		m_layer1Weights = nullptr;
		m_layer2Weights = nullptr;
		m_layer1Biases = nullptr;
		m_layer2Biases = nullptr;
		m_inputSize = tensorSize;
	}

	// Batch-norm (inference): y = (x - mean) / sqrt(variance + eps)
	// Note: stored per-channel; here channels == tensorSize and spatial == 1.
	float* m_bnMean = nullptr;     // size: inputSize
	float* m_bnVariance = nullptr; // size: inputSize
	float  m_bnEpsilon = 0.0f;

	// First FC: tensorSize -> SecondLayerSize
	// Second FC: SecondLayerSize -> (1 + cMaxNumMoves) [value + policy]
	float* m_layer1Weights = nullptr; // size: SecondLayerSize * inputSize
	float* m_layer1Biases = nullptr;  // size: SecondLayerSize
	float* m_layer2Weights = nullptr; // size: (1 + cMaxNumMoves) * SecondLayerSize
	float* m_layer2Biases = nullptr;  // size: 1 + cMaxNumMoves
	u32    m_inputSize = 0;

	void prepareAfterLoad() override
	{
		using bn_layer = tiny_dnn::batch_normalization_layer;
		using fc_layer = tiny_dnn::fully_connected_layer;

		// Layout:
		// [0] batch_norm
		// [1] fully_connected(tensorSize -> SecondLayerSize)
		// [2] relu
		// [3] fully_connected(SecondLayerSize -> 1 + cMaxNumMoves)
		// [4] sigmoid

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

		// Layer 1: first fully-connected
		if (m_net.layer_size() > 1) {
			auto* l1 = dynamic_cast<fc_layer*>(m_net[1]);
			if (l1) {
				auto& w1_ref = *l1->weights()[0];
				auto& b1_ref = *l1->weights()[1];

				tiny_dnn::vec_t& w1 = w1_ref;
				tiny_dnn::vec_t& b1 = b1_ref;

				m_layer1Weights = w1.empty() ? nullptr : w1.data();
				m_layer1Biases = b1.empty() ? nullptr : b1.data();

				m_inputSize = static_cast<u32>(l1->in_size());
			}
		}

		// Layer 3: second fully-connected
		if (m_net.layer_size() > 3) {
			auto* l3 = dynamic_cast<fc_layer*>(m_net[3]);
			if (l3) {
				auto& w3_ref = *l3->weights()[0];
				auto& b3_ref = *l3->weights()[1];

				tiny_dnn::vec_t& w3 = w3_ref;
				tiny_dnn::vec_t& b3 = b3_ref;

				m_layer2Weights = w3.empty() ? nullptr : w3.data();
				m_layer2Biases = b3.empty() ? nullptr : b3.data();
			}
		}
	}

	// Manual, allocation-light forward pass
	tiny_dnn::vec_t forward(const tiny_dnn::vec_t& x, void* pThreadContext, u32 netAge) override
	{
		if (!m_bnMean || !m_bnVariance || !m_layer1Weights || !m_layer2Weights || !m_layer1Biases || !m_layer2Biases) {
			return BaseNN::forward(x, pThreadContext, netAge);
		}

		DEBUG_ASSERT(x.size() == m_inputSize);

		constexpr u32 kOutSize = 1u + sevenWD::GameController::cMaxNumMoves;

		// BN output (spatial=1, channels=inputSize): bn[i] = (x[i] - mean[i]) / sqrt(var[i] + eps)
		float bnOut[sevenWD::GameState::TensorSize + sevenWD::GameState::ExtraTensorSize];

		DEBUG_ASSERT(m_inputSize <= (sizeof(bnOut) / sizeof(bnOut[0])));

		for (u32 i = 0; i < m_inputSize; ++i) {
			const float denom = std::sqrt(m_bnVariance[i] + m_bnEpsilon);
			bnOut[i] = (x[i] - m_bnMean[i]) / denom;
		}

		// First FC + ReLU: y1 = relu(W1 * bnOut + b1)
		float hidden[SecondLayerSize];

		for (u32 o = 0; o < SecondLayerSize; ++o) {
			float acc = m_layer1Biases[o];
			for (u32 i = 0; i < m_inputSize; ++i) {
				acc += m_layer1Weights[i * SecondLayerSize + o] * bnOut[i];
			}
			hidden[o] = acc > 0.0f ? acc : 0.0f;
		}

		// Second FC + sigmoid: out[o] = sigmoid(W2 * hidden + b2)
		tiny_dnn::vec_t result(kOutSize);
		for (u32 o = 0; o < kOutSize; ++o) {
			float acc = m_layer2Biases[o];
			for (u32 i = 0; i < SecondLayerSize; ++i) {
				acc += m_layer2Weights[i * kOutSize + o] * hidden[i];
			}
			result[o] = 1.0f / (1.0f + std::exp(-acc));
		}

		return result;
	}
};

#else

TODO

#endif

#ifdef USE_TINY_DNN

struct BaseLine : BaseNN
{
	BaseLine(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		u32 tensorSize = sevenWD::GameState::TensorSize +
			(useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

		m_net << tiny_dnn::fully_connected_layer(tensorSize, 1) << tiny_dnn::sigmoid_layer();
	}
};

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