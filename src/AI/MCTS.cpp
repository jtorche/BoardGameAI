
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

// --------------------------------------------------- //
// ---------------- MCTS_Deterministic --------------- //
sevenWD::Move MCTS_Deterministic::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	std::vector<float> scores(_moves.size(), 0.0f);

	core::LinearAllocator linAllocator(256 * 1024);

	MTCS_Node root(nullptr, {}, _game);
	root.m_gameState.m_gameState.makeDeterministic();

	for (unsigned int iter = 0; iter < m_numMoves; ++iter) {
		MTCS_Node* pSelectedNode = selection(&root);
		MTCS_Node* pExpandedNode = expansion(pSelectedNode, linAllocator);
		if (pExpandedNode) {
			auto [reward, simPlayer] = playout(pExpandedNode);
			backPropagate(pExpandedNode, reward);
		} else {
			u32 curPlayer = pSelectedNode->m_pParent->m_gameState.m_gameState.getCurrentPlayerTurn();
			GameController::State state = pSelectedNode->m_gameState.m_state;
			float reward = (state == GameController::State::WinPlayer0) ? (curPlayer == 0 ? 1.0f : 0.0f) : (state == GameController::State::WinPlayer1 ? (curPlayer == 1 ? 1.0f : 0.0f) : 0.0f);
			backPropagate(pSelectedNode->m_pParent, reward);
		}
	}
	
	// select best move from root
	u32 bestIndex = 0;
	u32 bestVisits = 0;
	for (u32 i = 0; i < root.m_numChildren; ++i) {
		MTCS_Node* child = root.m_children[i];
		if (child->m_visits > bestVisits) {
			bestVisits = child->m_visits;
			bestIndex = i;
		}
	}

	return root.m_children[bestIndex]->m_move_from_parent;
}

MTCS_Node* MCTS_Deterministic::selection(MTCS_Node* pNode)
{
	if (pNode->m_numChildren == 0 || pNode->m_numUnexploredMoves > 0) {
		return pNode;
	}
	else {
		// select best child
		float bestUCB = -1.0f;
		MTCS_Node* pBestChild = nullptr;
		for (u32 i = 0; i < pNode->m_numChildren; ++i) {
			MTCS_Node* pChild = pNode->m_children[i];
			float ucb = (pChild->m_totalRewards / (pChild->m_visits + cEpsilon)) +
				C * sqrtf(logf(static_cast<float>(pNode->m_visits) + 1.0f) / (pChild->m_visits + cEpsilon));
			if (ucb > bestUCB) {
				bestUCB = ucb;
				pBestChild = pChild;
			}
		}
		return selection(pBestChild);
	}
}

MTCS_Node* MCTS_Deterministic::expansion(MTCS_Node* pNode, core::LinearAllocator& linAllocator)
{
	if (pNode->m_gameState.m_winType != sevenWD::WinType::None) {
		return nullptr;
	}
	else {
		if (pNode->m_numUnexploredMoves == 0) {
			DEBUG_ASSERT(pNode->m_numChildren == 0);

			// initialize unexplored moves
			pNode->m_numUnexploredMoves = pNode->m_gameState.enumerateMoves(pNode->m_pUnexploredMoves, pNode->m_unexploredMoveStorage.size());
			DEBUG_ASSERT(pNode->m_numUnexploredMoves > 0);

			if (pNode->m_numUnexploredMoves > pNode->m_unexploredMoveStorage.size()) {
				pNode->m_pUnexploredMoves = new sevenWD::Move[pNode->m_numUnexploredMoves];
				pNode->m_children = new MTCS_Node*[pNode->m_numUnexploredMoves];
				pNode->m_gameState.enumerateMoves(pNode->m_pUnexploredMoves, pNode->m_numUnexploredMoves);
			}
		}

		// expand
		u32 moveIndex = m_rand() % pNode->m_numUnexploredMoves;
		sevenWD::Move move = pNode->m_pUnexploredMoves[moveIndex];
		// remove move from unexplored moves
		pNode->m_pUnexploredMoves[moveIndex] = pNode->m_pUnexploredMoves[pNode->m_numUnexploredMoves - 1];
		--pNode->m_numUnexploredMoves;
		// create child node
		sevenWD::GameController newGameState = pNode->m_gameState;
		newGameState.play(move);
		MTCS_Node* pChildNode = linAllocator.allocate<MTCS_Node>(pNode, move, newGameState);
		pNode->m_children[pNode->m_numChildren++] = pChildNode;
		return pChildNode;
	}
}

std::pair<float, u32> MCTS_Deterministic::playout(MTCS_Node* pNode)
{
	DEBUG_ASSERT(pNode->m_gameState.m_winType == sevenWD::WinType::None);

	sevenWD::GameController controller = pNode->m_gameState;
	bool end = false;
	std::vector<sevenWD::Move> moves;
	while (!end) {
		controller.enumerateMoves(moves);
		if (moves.empty()) {
			break;
		}
		sevenWD::Move move = moves[m_rand() % moves.size()];
		end = controller.play(move);
	}

	// compute reward from root player perspective
	const u32 rootPlayer = pNode->m_gameState.m_gameState.getCurrentPlayerTurn();
	float reward = 0.0f;
	if (controller.m_state == sevenWD::GameController::State::WinPlayer0 && rootPlayer == 0) {
		reward = 1.0f;
	}
	else if (controller.m_state == sevenWD::GameController::State::WinPlayer1 && rootPlayer == 1) {
		reward = 1.0f;
	}
	else {
		reward = 0.0f;
	}

	return { reward, rootPlayer };
}
void backPropagate(MTCS_Node* pNode, float reward)
{
	// playout reward is computed with respect to the player to move at pNode (playout player).
	// We store node rewards from the perspective of the player who made the move that led to that node.
	// Therefore, when propagating up the tree we must invert the reward at each ply.
	if (!pNode) return;

	const u32 playoutPlayer = pNode->m_gameState.m_gameState.getCurrentPlayerTurn();

	MTCS_Node* pCur = pNode;
	while (pCur != nullptr) {
		// owner is the player who made the move to reach 'cur'
		const u32 owner = 1 - pCur->m_gameState.m_gameState.getCurrentPlayerTurn();
		// if owner == playoutPlayer, the reward is directly the playout reward,
		// otherwise it is the inverted reward (opponent perspective).
		const float valueForOwner = (owner == playoutPlayer) ? reward : (1.0f - reward);

		pCur->m_visits++;
		pCur->m_totalRewards += valueForOwner;

		pCur = pCur->m_pParent;
	}
}