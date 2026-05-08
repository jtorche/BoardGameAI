#pragma once

#include "Core/Common.h"
#include "BuildConfig.h"
#include "AI.h"
#include "MinMaxAI.h"
#include <mutex>
#include <array>

enum class NetworkType {
	Net_BaseLine,
	Net_TwoLayer8,
	Net_TwoLayer24,
	Net_TwoLayer64,
	Net_TwoLayer4_PUCT,
	Net_TwoLayer8_PUCT,
	Net_TwoLayer16_PUCT,
	Net_TwoLayer32_PUCT,
	Net_ThreeLayer32_PUCT,
	Net_FourLayer32_PUCT,
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
		case NetworkType::Net_TwoLayer4_PUCT: return "TwoLayers4_PUCT";
		case NetworkType::Net_TwoLayer8_PUCT: return "TwoLayers8_PUCT";
		case NetworkType::Net_TwoLayer16_PUCT: return "TwoLayers16_PUCT";
		case NetworkType::Net_TwoLayer32_PUCT: return "TwoLayers32_PUCT";
		case NetworkType::Net_ThreeLayer32_PUCT: return "ThreeLayers32_PUCT";
		case NetworkType::Net_FourLayer32_PUCT: return "FourLayers32_PUCT";
		default: return "UnknownNet";
		}
	}

	NetworkType m_netType;
	bool m_extraTensorData;

	BaseNN(NetworkType netType, bool extraTensorData) : m_netType(netType), m_extraTensorData(extraTensorData) {}

#if defined(USE_TINY_DNN)
	using TinyDNN_Net = tiny_dnn::network<tiny_dnn::sequential>;
	tiny_dnn::network<tiny_dnn::sequential> m_net;

	virtual void prepareAfterLoad() {}
	virtual tiny_dnn::vec_t forward(const tiny_dnn::vec_t& x, void* pThreadContext, u32 netIndex);
	TinyDNN_Net& getNetwork() { return m_net; }
#else
	virtual torch::Tensor forward(torch::Tensor) { DEBUG_ASSERT(0); }
#endif
	
	const char* getNetName() const { return getNetworkName(m_netType); }
};

struct BaseNetworkAI : AIInterface, bg::MinMaxAIHeuristic {
	// Take std::array instead of C-style array
	BaseNetworkAI(std::string name, const std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& network) : m_name(std::move(name)), m_network(network) {
	}

	struct ThreadContext {
		const BaseNetworkAI* m_pThis;
		BaseNN::TinyDNN_Net m_net[bg::cNumNetworks];
		float m_puctPriors[bg::GameController::cMaxNumMoves] = { 0.f }; // Priors for PUCT search (used to train a NN-based MCTS AI)
	};

	float computeScore(const bg::GameController& state, u32 maxPlayer, void* pContext) const {
		auto& network = m_network[state.getNetId()];
		const u32 tensorSize = bg::GameController::TensorSize + (network->m_extraTensorData ? bg::GameController::ExtraTensorSize : 0);

#if defined(USE_TINY_DNN)
		tiny_dnn::vec_t buffer(tensorSize);
#else
		std::vector<float> buffer(tensorSize);
#endif
		state.fillTensorData(buffer.data(), 0);
		if (network->m_extraTensorData)
			state.fillExtraTensorData(buffer.data() + bg::GameController::TensorSize, 0);

#if defined(USE_TINY_DNN)
		ThreadContext* pThreadContext  = (ThreadContext*)pContext;
		DEBUG_ASSERT(pThreadContext == nullptr || pThreadContext->m_pThis == this);
		tiny_dnn::vec_t output = network->forward(buffer, pThreadContext, state.getNetId());
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
			char buffer[256];
			for (u32 i = 0; i < bg::cNumNetworks; ++i) {
				sprintf_s(buffer, "tmp%u_createPerThreadContext.bin", i);
				m_network[i]->getNetwork().save(buffer);
				pContext->m_net[i].load(buffer);
			}
			m_mutex.unlock();
			return pContext;
		}
		else if (needPUCTPriors()) {
			ThreadContext* pContext = new ThreadContext{ this };
			return pContext;

		}
		else {
			return nullptr;
		}
	}

	void destroyPerThreadContext(void* ptr) const override { delete (ThreadContext*)ptr; }

	std::string m_name;
	std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks> m_network;	
};

struct SimpleNetworkAI : BaseNetworkAI
{
	using BaseNetworkAI::BaseNetworkAI;

	float m_bestScoreMargin = 0.03f;

	std::string getName() const override {
		return "SimpleNetworkAI_" + m_name;
	}

	std::pair<bg::Move, float> selectMove(const bg::GameContext& _sevenWDContext, const bg::GameController& controller, const std::vector<bg::Move>& _moves, void* pThreadContext) override
	{
		std::vector<float> scores(_moves.size());

		for (u32 i = 0; i < _moves.size(); ++i) {
			bg::GameController tmpController = controller;
			bool endGame = tmpController.play(_moves[i]);
			if (endGame) {
				u32 winner = (tmpController.getWinner() == 0) ? 0 : 1;
				scores[i] = (controller.getCurrentPlayerTurn() == winner) ? 1.0f : 0.0f;
			}
			else {
				u32 curPlayer = controller.getCurrentPlayerTurn();
				scores[i] = computeScore(tmpController, curPlayer, pThreadContext);
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
			bg::SerializableGameState m_state;
			u32 m_winner;
			bg::WinType m_winType;
			float m_puctPriors[bg::GameController::cMaxNumMoves];
		};

		std::vector<Point> m_data;

		void clear() { m_data.clear(); }

		void printStats();

		void prepareForTraining(const bg::GameContext& sevenWDContext, u32 (&victoryTypeWeight)[(u32)bg::WinType::Count]);

		void operator+=(const Dataset& dataset) {
			for (const Point& d : dataset.m_data)
				m_data.push_back(d);
		}

		void fillBatches(u32 batchSize, std::vector<Batch>& batches, bool useExtraTensorData, bool usePUCT) const;
#if defined(USE_TINY_DNN)
		void fillBatches(bool useExtraTensorData, bool usePUCT, tiny_dnn::tensor_t& outData, tiny_dnn::tensor_t& outLabels) const;
#endif

		bool saveToFile(const std::string& filename) const;
		bool loadFromFile(const bg::GameContext& context, const std::string& filename);
	};

	static u32 generateOneGameDatasSet(const bg::GameContext& sevenWDContext,
		AIInterface* AIs[2], void* AIThreadContexts[2], std::vector<Dataset::Point>(&data)[bg::cNumNetworks], bg::WinType& winType, double(&thinkingTime)[2]);

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
	static void saveNet(std::string namePrefix, u32 generation, const std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net);
	static bool loadNet(NetworkType netType, std::string namePrefix, u32 generation, std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net, bool useExtraTensorData);
	static bool loadLastGenNet(NetworkType netType, std::string namePrefix, bool useExtraTensorData, u32& outGeneration, std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net, std::string& outFullName);

	template<typename T>
	static std::pair<T*, u32> loadAIFromFile(NetworkType netType, std::string namePrefix, bool useExtraTensorData)
	{
		u32 mostRecentGen = 0;
		std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks> net;
		std::string fullName;
		if (loadLastGenNet(netType, namePrefix, useExtraTensorData, mostRecentGen, net, fullName)) {
			return std::make_pair(new T(fullName, net), mostRecentGen);
		}
		return {};
	}
};