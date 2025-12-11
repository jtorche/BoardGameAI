#pragma once

#include "7WDuel/GameController.h"
#include "AI.h"

namespace sevenWD
{
	//-------------------------------------------------------------------------------------------------
	struct MinMaxAIHeuristic {
		virtual float computeScore(const GameState& _gameState, u32 _maxPlayer, void* pThreadContext) const = 0;
	};

	//-------------------------------------------------------------------------------------------------
	class MinMaxAI : public AIInterface
	{
	public:
		MinMaxAI(const MinMaxAIHeuristic* pHeuristic, u32 _maxDepth = 10, bool _monothread = false);


		double getAvgMovesPerTurn() const { return double(m_numMoves.load()) / m_numNodeExplored.load(); }

		Move selectMove(const GameContext& _sevenWDContext, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext) override;

		std::string getName() const {
			return "MinMax";
		}

	private:
		float evalRec(u32 _maxPlayer, const GameController& _gameState, u32 _depth, vec2 _a_b, void* pThreadContext);
		// float computeScore(const GameController& _gameState, u32 _maxPlayer) const;

	private:
		const MinMaxAIHeuristic* m_pHeuristic;
		u32 m_maxDepth = 10;
		bool m_monothread = false;
		std::atomic<u64> m_numNodeExplored = 0, m_numMoves = 0, n_numLeafEpxlored = 0;
		std::atomic<bool> m_stopAI;
	};
}
