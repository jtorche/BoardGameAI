#include "AI.h"

std::pair<bg::Move, float> MonteCarloAI::selectMove(const bg::GameContext& _sevenWDContext, const bg::GameController& _game, const std::vector<bg::Move>& _moves, void* pThreadContext) {
	std::vector<u32> numWins(_moves.size());

	std::vector<bg::Move> curMoves;
	for (u32 i = 0; i < _moves.size(); ++i) {
		for (u32 j = 0; j < m_numSimu; ++j) {
			bg::GameController game = _game;
			bool end = game.play(_moves[i]);
			while (!end) {
				game.enumerateMoves(curMoves);
				end = game.play(curMoves[_sevenWDContext.rand()() % curMoves.size()]);
			}
			if (game.getWinner() == 0 && _game.getCurrentPlayerTurn() == 0) {
				numWins[i]++;
			}
			else if (game.getWinner() == 1 && _game.getCurrentPlayerTurn() == 1) {
				numWins[i]++;
			}
		}
	}

	auto it = std::max_element(numWins.begin(), numWins.end());
	return { _moves[std::distance(numWins.begin(), it)], ((float)*it) / m_numSimu };
}