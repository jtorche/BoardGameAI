
#include "MCTS.h"

// --------------------------------------------- //
// ---------------- MCTS_Simple ---------------- //

sevenWD::Move MCTS_Simple::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	std::vector<float> scores(_moves.size());
	std::vector<Move> curMoves;
	unsigned int rootPlayer = _game.m_gameState.getCurrentPlayerTurn();

	for (u32 i = 0; i < _moves.size(); ++i) {
		for (u32 j = 0; j < m_numSimu; ++j) {
			GameController game = _game;
			bool end = game.play(_moves[i]);
			unsigned int depth = 0;
			while (!end && ++depth < m_depth) {
				game.enumerateMoves(curMoves);
				end = game.play(curMoves[_sevenWDContext.rand()() % curMoves.size()]);
			}

			float s = 0;
			if (end) {
				if (game.m_state == GameController::State::WinPlayer0 && rootPlayer == 0) {
					s = 1.0;
				}
				else if (game.m_state == GameController::State::WinPlayer1 && rootPlayer == 1) {
					s = 1.0;
				}
				else {
					s = 0.0;
				}
			}
			else {
				s = computeScore(game.m_gameState, rootPlayer, pThreadContext);
			}

			scores[i] += s;
		}
	}

	auto it = std::max_element(scores.begin(), scores.end());
	return _moves[std::distance(scores.begin(), it)];
}

// --------------------------------------------- //
// ---------------- MCTS_Guided ---------------- //

sevenWD::Move MCTS_Guided::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	std::vector<std::pair<float, int>> numWins(_moves.size(), { 0.0f, 0 });
	std::vector<Move> moves;
	std::vector<float> scores;

	const int rootPlayer = _game.m_gameState.getCurrentPlayerTurn();

	for (unsigned int sim = 0; sim < m_numSimu; ++sim) {
		bool gameFinished = false;
		unsigned int curDepth = 0;
		moves = _moves;
		GameController game = _game;

		int firstMoveIndex = -1;

		do {

			scores.clear();
			float total = 0;

			const int currentPlayer = game.m_gameState.getCurrentPlayerTurn();

			for (const Move& move : moves) {

				GameController gameAfterMove = game;
				bool end = gameAfterMove.play(move);

				float s = 0;

				if (end) {
					if (gameAfterMove.m_state == GameController::State::WinPlayer0 && currentPlayer == 0) {
						s = 1.0f;
					}
					else if (gameAfterMove.m_state == GameController::State::WinPlayer1 && currentPlayer == 1) {
						s = 1.0f;
					}
					else {
						s = 0.0f;
					}
				}
				else {
					s = computeScore(gameAfterMove.m_gameState, currentPlayer, pThreadContext);
				}

				scores.push_back(s);
				total += s;
			}

			// avoid zero score distribution issues
			if (total <= 0.0f) {
				for (float& s : scores) s = 1.0f;
				total = (float)scores.size();
			}

			// Sample the move proportionally to score
			std::uniform_real<float> uniformFloat(0.0, total);
			float density = uniformFloat(_sevenWDContext.rand());

			unsigned int i;
			for (i = 0; i < scores.size(); ++i) {
				if ((density - scores[i]) < 0.0f) {
					break;
				}
				density -= scores[i];
			}

			i = std::min(i, (unsigned int)(moves.size() - 1));

			if (firstMoveIndex < 0)
				firstMoveIndex = i;

			gameFinished = game.play(moves[i]);
			curDepth++;

			if (!gameFinished)
				game.enumerateMoves(moves);

		} while (!gameFinished && curDepth < m_depth);


		// -----------------------
		//     BACKPROP REWARD
		// -----------------------
		// reward is computed from ROOT PLAYER perspective
		float reward = 0.0f;

		if (game.m_state == GameController::State::WinPlayer0 && rootPlayer == 0) {
			reward = 1.0f;
		}
		else if (game.m_state == GameController::State::WinPlayer1 && rootPlayer == 1) {
			reward = 1.0f;
		}
		else if (game.m_state == GameController::State::WinPlayer0 || game.m_state == GameController::State::WinPlayer1) {
			// root player lost
			reward = 0.0f;
		}
		else {
			// game NOT finished -> evaluate final simulated state from ROOT player perspective
			reward = computeScore(game.m_gameState, rootPlayer, pThreadContext);
		}

		if (firstMoveIndex >= 0) {
			numWins[firstMoveIndex].first += reward;
			numWins[firstMoveIndex].second += 1;
		}
	}

	// -----------------------
	//     FINAL SELECTION
	// -----------------------
	float bestScore = -1.0f;
	unsigned int bestIndex = 0;

	for (unsigned int i = 0; i < numWins.size(); ++i) {
		float w = numWins[i].first;
		int n = numWins[i].second;

		float avg = (n > 0 ? w / n : 0.0f);

		if (avg > bestScore) {
			bestScore = avg;
			bestIndex = i;
		}
	}

	return _moves[bestIndex];
}