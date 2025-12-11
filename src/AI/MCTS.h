#pragma once
#include "ML.h"

struct MTCS_Node {

	sevenWD::GameState m_gameState;
	MTCS_Node* m_pParent;
	sevenWD::Move move_from_parent;
	std::vector<sevenWD::Move> m_unexploredMoves;
	std::vector<std::unique_ptr<MTCS_Node>> m_children;

	int visits = 0;
	float totalRewards = 0;
};

struct MCTS_Simple : BaseNetworkAI
{
	using BaseNetworkAI::BaseNetworkAI;

	unsigned int m_numSimu = 20;
	unsigned int m_depth = 8;

	std::string getName() const override {
		return "MCTS_Simple_" + m_name + "_sim" + std::to_string(m_numSimu) + "_d" + std::to_string(m_depth);
	}

	sevenWD::Move selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;
};

struct MCTS_Guided : BaseNetworkAI
{
	using BaseNetworkAI::BaseNetworkAI;

	unsigned int m_numSimu = 20;
	unsigned int m_depth = 20;

	std::string getName() const override {
		return "MCTS_Guided_" + m_name + "_sim" + std::to_string(m_numSimu) + "_d" + std::to_string(m_depth);
	}

	sevenWD::Move selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext) override;
};