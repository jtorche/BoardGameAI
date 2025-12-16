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
	{}

	MTCS_Node* m_pParent = nullptr;
	sevenWD::Move m_move_from_parent{};
	sevenWD::GameController m_gameState;

	std::array<sevenWD::Move, 16> m_unexploredMoveStorage;
	sevenWD::Move* m_pUnexploredMoves = m_unexploredMoveStorage.data();
	
	std::array<MTCS_Node*, 16> m_childrenStorage;
	MTCS_Node** m_children = m_childrenStorage.data();

	u16 m_numUnexploredMoves = 0;
	u16 m_numChildren = 0;

	u32 m_visits = 0;
	float m_totalRewards = 0;

	void cleanup() {
		if (m_pUnexploredMoves != m_unexploredMoveStorage.data()) {
			delete[] m_pUnexploredMoves;
		}
		for (u32 i = 0; i < m_numChildren; ++i) {
			m_children[i]->cleanup();
		}
		if (m_children != m_childrenStorage.data()) {
			delete[] m_children;
		}
		m_children = nullptr;
		m_pUnexploredMoves = nullptr;
	}
};


struct MCTS_Deterministic : sevenWD::AIInterface
{
	std::mt19937 m_rand{ (u32)time(nullptr) };
	u32 m_numMoves = 1000;
	u32 m_numSampling = 50;

	float C = sqrtf(2.0f); // exploration constant
	static constexpr float cEpsilon = 1e-5f;

	MCTS_Deterministic(u32 numMoves, u32 numGameState) : m_numMoves(numMoves), m_numSampling(numGameState) {}

	std::string getName() const override {
		return std::string("MCTS_Deterministic") + "_m" + std::to_string(m_numMoves) + "_s" + std::to_string(m_numSampling);
	}

	std::pair<sevenWD::Move, float> selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;

	void initRoot(MTCS_Node* pNode, const sevenWD::Move moves[], u32 numMoves, core::LinearAllocator& linAllocator);
	MTCS_Node* selection(MTCS_Node* pNode);
	MTCS_Node* expansion(MTCS_Node* pNode, core::LinearAllocator& linAllocator);
	std::pair<float, u32> playout(MTCS_Node* pNode);
	void backPropagate(MTCS_Node* pNode, float reward);
};