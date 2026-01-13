#include "AI.h"

namespace sevenWD
{
	std::pair<Move, float> NoBurnAI::selectMove(const GameContext& _sevenWDContext, const GameController&, const std::vector<Move>& _moves, void* pThreadContext)
	{
		std::vector<Move> cpyMoves = _moves;
		std::sort(cpyMoves.begin(), cpyMoves.end(), [&](const Move& _a, const Move& _b)
			{
				bool isBurnA = _a.action == Move::Burn;
				bool isBurnB = _b.action == Move::Burn;
				return (isBurnA == isBurnB) ? false : !isBurnA;
			});

		size_t i = 0;
		for (; i < cpyMoves.size(); ++i) {
			if (cpyMoves[i].action == Move::Burn) {
				break;
			}
		}

		if (i == 0) {
			return { _moves[_sevenWDContext.rand()() % _moves.size()], 0.0f };
		}
		else {
			return { i == 1 ? cpyMoves[0] : cpyMoves[_sevenWDContext.rand()() % (i - 1)], 0.0f };
		}
	}

	std::pair<Move, float> PriorityAI::selectMove(const GameContext&, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext)
	{
		auto moveScore = [&](const Move& m) {
			std::array<float, (size_t)CardType::Count> priority[3] = {};
			priority[0][(int)CardType::Grey] = 1.0f;
			priority[0][(int)CardType::Brown] = 0.9f;
			priority[0][(int)CardType::Yellow] = 0.8f;
			priority[0][(int)CardType::Blue] = 0.6f;
			priority[0][(int)CardType::Military] = 0.1f;
			priority[0][(int)CardType::Science] = m_focusScience ? 1.0f : 0.0f;

			priority[1][(int)CardType::Yellow] = 0.95f;
			priority[1][(int)CardType::Blue] = 0.93f;
			priority[1][(int)CardType::Grey] = 0.9f;
			priority[1][(int)CardType::Brown] = 0.8f;
			priority[1][(int)CardType::Wonder] = 0.1f;
			priority[1][(int)CardType::Military] = m_focusMilitary ? 1.0f : 0.0f;
			priority[1][(int)CardType::Science] = m_focusScience ? 1.0f : 0.0f;

			priority[2][(int)CardType::Blue] = 0.95f;
			priority[2][(int)CardType::Guild] = 0.9f;
			priority[2][(int)CardType::Wonder] = 0.8f;
			priority[2][(int)CardType::Military] = m_focusMilitary ? 1.0f : 0.0f;
			priority[2][(int)CardType::Science] = m_focusScience ? 1.0f : 0.0f;

			float score = 0;
			if (m.action == Move::Pick) {
				score += 10.0f;
				u32 age = _game.m_gameState.getCurrentAge();
				CardType type = _game.m_gameState.getPlayableCard(m.playableCard).getType();
				score += priority[age][(int)type];
			}
			else if (m.action == Move::BuildWonder || m.action == Move::BuildMausoleum) {
				score += 10.0f;
				u32 age = _game.m_gameState.getCurrentAge();
				score += priority[age][(int)CardType::Wonder];
			}

			return score;
		};

		std::vector<Move> cpyMoves = _moves;
		std::sort(cpyMoves.begin(), cpyMoves.end(), [&](const Move& _a, const Move& _b)
			{
				return moveScore(_a) > moveScore(_b);
			});

		return { cpyMoves[0], 0.0f };
	}

	std::pair<Move, float> MonteCarloAI::selectMove(const GameContext& _sevenWDContext, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext) {
		std::vector<u32> numWins(_moves.size());

		std::vector<Move> curMoves;
		for (u32 i = 0; i < _moves.size(); ++i) {
			for (u32 j = 0; j < m_numSimu; ++j) {
				GameController game = _game;
				bool end = game.play(_moves[i]);
				while (!end) {
					game.enumerateMoves(curMoves);
					end = game.play(curMoves[_sevenWDContext.rand()() % curMoves.size()]);
				}
				if (game.m_gameState.m_state == GameState::State::WinPlayer0 && _game.m_gameState.getCurrentPlayerTurn() == 0) {
					numWins[i]++;
				}
				else if (game.m_gameState.m_state == GameState::State::WinPlayer1 && _game.m_gameState.getCurrentPlayerTurn() == 1) {
					numWins[i]++;
				}
			}
		}

		auto it = std::max_element(numWins.begin(), numWins.end());
		return { _moves[std::distance(numWins.begin(), it)], ((float)*it) / m_numSimu };
	}
}