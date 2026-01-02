#pragma once
#include "ML.h"
#include "Core/LinearAllocator.h"

struct MCTS_Simple : BaseNetworkAI
{
	using BaseNetworkAI::BaseNetworkAI;

	unsigned int m_numSimu = 20;
	unsigned int m_depth = 8;

	std::string getName() const override {
		return "MCTS_Simple_" + m_name + "_sim" + std::to_string(m_numSimu) + "_d" + std::to_string(m_depth);
	}

	std::pair<sevenWD::Move, float> selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;
};

struct MTCS_Node {

	MTCS_Node(MTCS_Node* pParent, sevenWD::Move moveFromParent, const sevenWD::GameController& _gamestate)
		: m_pParent(pParent)
		, m_move_from_parent(moveFromParent)
		, m_gameState(_gamestate)
		, m_playerTurn((u8)_gamestate.m_gameState.getCurrentPlayerTurn())
	{}

	MTCS_Node* m_pParent = nullptr;
	sevenWD::Move m_move_from_parent{};
	sevenWD::GameController m_gameState;

	std::array<sevenWD::Move, 24> m_moveStorage;
	sevenWD::Move* m_pMoves = m_moveStorage.data();
	
	std::array<MTCS_Node*, 24> m_childrenStorage;
	MTCS_Node** m_children = m_childrenStorage.data();

	u16 m_numUnexploredMoves = 0;
	u8 m_numChildren = 0;
	u8 m_playerTurn = 0;

	float m_nnHeuristic = 0.0f;
	u32 m_visits = 0;
	float m_totalRewards = 0;
	float m_puctPriors[sevenWD::GameController::cMaxNumMoves] = { 0.0f };

	void cleanup() {
		if (m_pMoves != m_moveStorage.data()) {
			delete[] m_pMoves;
		}
		for (u32 i = 0; i < m_numChildren; ++i) {
			if (m_children[i]) {
				m_children[i]->cleanup();
			}
		}
		if (m_children != m_childrenStorage.data()) {
			delete[] m_children;
		}
		m_children = nullptr;
		m_pMoves = nullptr;
	}
};

class thread_pool;
struct MCTS_Deterministic : BaseNetworkAI
{
	enum HeuristicType {
		Heuristic_RandomRollout,
		Heuristic_UseDNN,
		Heuristic_NoBurnRollout,
	};

	std::mt19937 m_rand{ (u32)time(nullptr) };
	thread_pool* m_threadPool = nullptr;
	u32 m_numMoves = 1000;
	u32 m_numSampling = 50;
	HeuristicType m_heuristic = Heuristic_RandomRollout;

	float C = sqrtf(2.0f);
	static constexpr float cEpsilon = 1e-5f;

	using BaseNetworkAI::BaseNetworkAI;
	MCTS_Deterministic(u32 numMoves, u32 numGameState, bool mt = false);

	std::string getName() const override {
		if (m_heuristic == Heuristic_UseDNN) {
			return std::string("MCTS_Deterministic_DNN") + "_m" + std::to_string(m_numMoves) + "_s" + std::to_string(m_numSampling);
		}
		else {
			return std::string((m_heuristic == Heuristic_NoBurnRollout) ? "MCTS_DeterministicNoBurn" : "MCTS_Deterministic") + "_m" + std::to_string(m_numMoves) + "_s" + std::to_string(m_numSampling);
		}
	}

	std::pair<sevenWD::Move, float> selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;

	void initRoot(MTCS_Node* pNode, const sevenWD::Move moves[], u32 numMoves, core::LinearAllocator& linAllocator);
	MTCS_Node* selection(MTCS_Node* pNode, u32& depth);
	MTCS_Node* expansion(MTCS_Node* pNode, core::LinearAllocator& linAllocator);
	std::pair<float, u32> playout(MTCS_Node* pNode, std::vector<sevenWD::Move>& scratchMoves, void* pThreadContext);
	void backPropagate(MTCS_Node* pNode, float reward);
};

struct MCTS_Zero : BaseNetworkAI
{
	std::mt19937 m_rand{ (u32)time(nullptr) };
	thread_pool* m_threadPool = nullptr;
	u32 m_numMoves = 1000;
	u32 m_numSampling = 50;
	float C = 2.0f;
	float m_scienceBoost = 0.0f;
	bool m_useNNHeuristic = true;
	bool m_useDirichletNoise = true;
	bool m_useTemperature = true;
	static constexpr float cEpsilon = 1e-5f;

	using BaseNetworkAI::BaseNetworkAI;
	MCTS_Zero(u32 numMoves, u32 numGameState, bool mt = false);

	void enableMT();

	std::string getName() const override {
		return std::string("MCTS_Zero") + "_m" + std::to_string(m_numMoves) + "_s" + std::to_string(m_numSampling) + ((m_scienceBoost>0) ? "_sc" : "");
	}

	std::pair<sevenWD::Move, float> selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;

	void fillPUCTPriors(void* pContext, float(&puctPriors)[sevenWD::GameController::cMaxNumMoves]) override {
		ThreadContext* pThreadContext = (ThreadContext*)pContext;
		DEBUG_ASSERT(pThreadContext == nullptr || pThreadContext->m_pThis == this);

		if (pThreadContext) {
			memcpy(puctPriors, pThreadContext->m_puctPriors, sizeof(puctPriors));
		}
		else {
			memset(puctPriors, 0, sizeof(puctPriors));
		}
	}

	bool needPUCTPriors() const override { return true; }

	void initPUCTPriors(MTCS_Node* pNode, void* pThreadContext, const sevenWD::Move moves[], u32 numMoves) const;
	void computeNNInference(MTCS_Node* pNode, void* pThreadContext) const;
	void initRoot(MTCS_Node* pNode, const sevenWD::Move moves[], u32 numMoves, core::LinearAllocator& linAllocator, void* pThreadContext);
	MTCS_Node* selection(MTCS_Node* pNode, u32& depth, core::LinearAllocator& linAllocator, void* pThreadContext);
	std::pair<float, u32> playout(MTCS_Node* pNode, std::vector<sevenWD::Move>& scratchMoves, void* pThreadContext);
	void backPropagate(MTCS_Node* pNode, float reward);
};