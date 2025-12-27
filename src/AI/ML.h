#pragma once

#include "Core/Common.h"
#include "7WDuel/GameController.h"
#include "AI.h"
#include "MinMaxAI.h"
#include <mutex>
#include <array>

enum class NetworkType {
	Net_BaseLine,
	Net_TwoLayer8,
	Net_TwoLayer24,
	Net_TwoLayer64,
};

struct BaseNN 
#if !defined(USE_TINY_DNN)
	: torch::nn::Module 
#endif
{

	static const char* getNetworkName(NetworkType netType) {
		switch (netType) {
		case NetworkType::Net_BaseLine: return "BaseLine";
		case NetworkType::Net_TwoLayer8: return "TwoLayers8";
		case NetworkType::Net_TwoLayer24: return "TwoLayers24";
		case NetworkType::Net_TwoLayer64: return "TwoLayers64";
		default: return "UnknownNet";
		}
	}

	NetworkType m_netType;
	bool m_extraTensorData;

	BaseNN(NetworkType netType, bool extraTensorData) : m_netType(netType), m_extraTensorData(extraTensorData) {}

#if defined(USE_TINY_DNN)
	using TinyDNN_Net = tiny_dnn::network<tiny_dnn::sequential>;
	tiny_dnn::network<tiny_dnn::sequential> m_net;

    tiny_dnn::vec_t forward(const tiny_dnn::vec_t& x) { return m_net.predict(x); }
	TinyDNN_Net& getNetwork() { return m_net; }
#else
	virtual torch::Tensor forward(torch::Tensor) { DEBUG_ASSERT(0); }
#endif
	
	const char* getNetName() const { return getNetworkName(m_netType); }
};

struct BaseNetworkAI : sevenWD::AIInterface, sevenWD::MinMaxAIHeuristic {
	// Take std::array instead of C-style array
	BaseNetworkAI(std::string name, const std::array<std::shared_ptr<BaseNN>, 3>& network) : m_name(std::move(name)), m_network(network) {
	}

	struct ThreadContext {
		const BaseNetworkAI* m_pThis;
		BaseNN::TinyDNN_Net m_net[3];
	};

	float computeScore(const sevenWD::GameState& state, u32 maxPlayer, void* pContext) const {
		u8 age = (u8)state.getCurrentAge();
		age = age == u8(-1) ? 0 : age;
		auto& network = m_network[age];

		const u32 tensorSize = sevenWD::GameState::TensorSize + (network->m_extraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

#if defined(USE_TINY_DNN)
		tiny_dnn::vec_t buffer(tensorSize);
#else
		std::vector<float> buffer(tensorSize);
#endif
		state.fillTensorData(buffer.data(), 0);
		if (network->m_extraTensorData)
			state.fillExtraTensorData(buffer.data() + sevenWD::GameState::TensorSize);

#if defined(USE_TINY_DNN)
		ThreadContext* pThreadContext  = (ThreadContext*)pContext;
		DEBUG_ASSERT(pThreadContext == nullptr || pThreadContext->m_pThis == this);
		tiny_dnn::vec_t output = pThreadContext ? pThreadContext->m_net[age].predict(buffer) : network->forward(buffer);
		float player0WinProbability = output[0];
#else
		torch::Tensor result = network->forward(torch::from_blob(buffer.data(), { 1, tensorSize }, torch::kFloat));
		float player0WinProbability = result[0].item<float>();
#endif
		return maxPlayer == 0 ? player0WinProbability : 1.0f - player0WinProbability;
	}

	void* createPerThreadContext() const override {
		static std::mutex m_mutex;

		if (m_network[0] && m_network[1] && m_network[2]) {
			ThreadContext* pContext = new ThreadContext{ this };
			m_mutex.lock();
			m_network[0]->getNetwork().save("tmp0_createPerThreadContext.bin");
			m_network[1]->getNetwork().save("tmp1_createPerThreadContext.bin");
			m_network[2]->getNetwork().save("tmp2_createPerThreadContext.bin");
			pContext->m_net[0].load("tmp0_createPerThreadContext.bin");
			pContext->m_net[1].load("tmp1_createPerThreadContext.bin");
			pContext->m_net[2].load("tmp2_createPerThreadContext.bin");
			m_mutex.unlock();
			return pContext;
		}

		return nullptr;
	}
	void destroyPerThreadContext(void* ptr) const override { delete (ThreadContext*)ptr; }

	std::string m_name;
	// Use std::array for network storage
	std::array<std::shared_ptr<BaseNN>, 3> m_network;	
};

struct SimpleNetworkAI : BaseNetworkAI
{
	using BaseNetworkAI::BaseNetworkAI;

	float m_bestScoreMargin = 0.03f;

	std::string getName() const override {
		return "SimpleNetworkAI_" + m_name;
	}

	std::pair<sevenWD::Move, float> selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& controller, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override
	{
		std::vector<float> scores(_moves.size());

		for (u32 i = 0; i < _moves.size(); ++i) {
			sevenWD::GameController tmpController = controller;
			bool endGame = tmpController.play(_moves[i]);
			if (endGame) {
				u32 winner = (tmpController.m_state == sevenWD::GameController::State::WinPlayer0) ? 0 : 1;
				scores[i] = (controller.m_gameState.getCurrentPlayerTurn() == winner) ? 1.0f : 0.0f;
			}
			else {
				u32 curPlayer = controller.m_gameState.getCurrentPlayerTurn();
				scores[i] = computeScore(tmpController.m_gameState, curPlayer, pThreadContext);
			}
		}

		auto it = std::max_element(scores.begin(), scores.end());
		u32 numEligibleScore = 0;
		if (m_bestScoreMargin > 0) {
			for (u32 i = 0; i < scores.size(); ++i) {
				if (scores[i] >= *it - m_bestScoreMargin) {
					numEligibleScore++;
				}
			}
		}

		if (numEligibleScore > 0) {
			u32 choice = _sevenWDContext.rand()() % numEligibleScore;
			for (u32 i = 0; i < scores.size(); ++i) {
				if (scores[i] >= *it - m_bestScoreMargin) {
					if (--numEligibleScore == choice) {
						return { _moves[i], scores[i] };
					}
				}
			}
		} 
		return { _moves[std::distance(scores.begin(), it)], (float)*it };
	}
};
struct ML_Toolbox
{
	struct Batch {
#ifdef USE_TINY_DNN
		std::vector<tiny_dnn::vec_t> data;
		std::vector<tiny_dnn::vec_t> labels;
#else
		torch::Tensor data;
		torch::Tensor labels;
#endif
	};

	struct Dataset {
		struct Point {
			sevenWD::GameState m_state;
			u32 m_winner;
			sevenWD::WinType m_winType;
		};

		std::vector<Point> m_data;

		void clear() { m_data.clear(); }

		void shuffle(const sevenWD::GameContext& sevenWDContext) {
			std::shuffle(m_data.begin(), m_data.end(), sevenWDContext.rand());
		}

		void operator+=(const Dataset& dataset) {
			for (const Point& d : dataset.m_data)
				m_data.push_back(d);
		}

		void fillBatches(u32 batchSize, std::vector<Batch>& batches, bool useExtraTensorData) const;
#if defined(USE_TINY_DNN)
		void fillBatches(bool useExtraTensorData, tiny_dnn::tensor_t& outData, tiny_dnn::tensor_t& outLabels) const;
#endif

		bool saveToFile(const std::string& filename) const;
		bool loadFromFile(const sevenWD::GameContext& context, const std::string& filename);
	};

	static u32 generateOneGameDatasSet(const sevenWD::GameContext& sevenWDContext,
		sevenWD::AIInterface* AIs[2], void* AIThreadContexts[2], std::vector<sevenWD::GameState>(&data)[3], sevenWD::WinType& winType, double(&thinkingTime)[2]);

#ifdef USE_TINY_DNN
	static void fillTensors(const Dataset& dataset, std::vector<tiny_dnn::vec_t>& outData, std::vector<tiny_dnn::vec_t>& outLabels);
	static float evalPrecision(const std::vector<tiny_dnn::vec_t>& predictions, const std::vector<tiny_dnn::vec_t>& labels);
	static std::pair<float, float> evalMeanLoss(const std::vector<tiny_dnn::vec_t>& predictions, const std::vector<tiny_dnn::vec_t>& labels, const std::vector<float>& weights);
#else
	static void fillTensors(const Dataset& dataset, torch::Tensor& outData, torch::Tensor& outLabels);
	static float evalPrecision(torch::Tensor predictions, torch::Tensor labels);
	static std::pair<float, float> evalMeanLoss(torch::Tensor predictions, torch::Tensor labels, torch::Tensor weights);
#endif

	static std::string buildNetFilename(std::string netName, std::string namePrefix, bool useExtraTensorData, u32 age, u32 generation);
	static u32 parseGenerationFromNetFilename(std::string filename);

	static std::shared_ptr<BaseNN> constructNet(NetworkType type, bool hasExtraData);

	static void trainNet(u32 age, u32 epoch, const std::vector<Batch>& batches, BaseNN* pNet, float alpha = 1e-3f);
	static void trainNet(u32 age, u32 epoch, tiny_dnn::tensor_t& data, tiny_dnn::tensor_t& labels, BaseNN* pNet);
	// APIs now use std::array instead of C arrays
	static void saveNet(std::string namePrefix, u32 generation, const std::array<std::shared_ptr<BaseNN>, 3>& net);
	static bool loadNet(NetworkType netType, std::string namePrefix, u32 generation, std::array<std::shared_ptr<BaseNN>, 3>& net, bool useExtraTensorData);
	static bool loadLastGenNet(NetworkType netType, std::string namePrefix, bool useExtraTensorData, u32& outGeneration, std::array<std::shared_ptr<BaseNN>, 3>& net, std::string& outFullName);

	template<typename T>
	static std::pair<T*, u32> loadAIFromFile(NetworkType netType, std::string namePrefix, bool useExtraTensorData)
	{
		u32 mostRecentGen = 0;
		std::array<std::shared_ptr<BaseNN>, 3> net{ nullptr, nullptr, nullptr };
		std::string fullName;
		if (loadLastGenNet(netType, namePrefix, useExtraTensorData, mostRecentGen, net, fullName)) {
			return std::make_pair(new T(fullName, net), mostRecentGen);
		}
		return {};
	}
};

#ifdef USE_TINY_DNN

template<u32 SecondLayerSize>
struct TwoLayers : BaseNN
{
	TwoLayers(NetworkType netType, bool useExtraTensorData)
		: BaseNN(netType, useExtraTensorData)
	{
		u32 tensorSize = sevenWD::GameState::TensorSize +
			(useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

		m_net << tiny_dnn::fully_connected_layer(tensorSize, SecondLayerSize)
			  << tiny_dnn::relu_layer()
			  << tiny_dnn::fully_connected_layer(SecondLayerSize, 1)
			  << tiny_dnn::sigmoid_layer();
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