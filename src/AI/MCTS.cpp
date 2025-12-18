
#include "MCTS.h"
#include "Core/thread_pool.h"

// --------------------------------------------- //
// ---------------- MCTS_Simple ---------------- //
// --------------------------------------------- //
std::pair<sevenWD::Move, float> MCTS_Simple::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
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
	return { _moves[std::distance(scores.begin(), it)], ((float)*it) / m_numSimu };
}

// --------------------------------------------------- //
// ---------------- MCTS_Deterministic --------------- //
// --------------------------------------------------- //
MCTS_Deterministic::MCTS_Deterministic(u32 numMoves, u32 numGameState, bool mt) : m_numMoves(numMoves), m_numSampling(numGameState)
{
	if (mt) {
		m_threadPool = new thread_pool(std::thread::hardware_concurrency());
	}
}

std::pair<sevenWD::Move, float> MCTS_Deterministic::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	std::vector<u32> sampledVisits(_moves.size(), 0);
	std::vector<float> scores(_moves.size(), 0);
	std::mutex* pMutex = nullptr;

	auto processRange = [&](u32 start, u32 end)
	{
		core::LinearAllocator linAllocator(8 * 1024 * 1024);
		std::vector<Move> scratchMoves;

		for (u32 i = start; i < end; ++i) {
			MTCS_Node* pRoot = linAllocator.allocate<MTCS_Node>((MTCS_Node*)nullptr, Move{}, _game);
			pRoot->m_gameState.m_gameState.makeDeterministic();
			initRoot(pRoot, _moves.data(), (u32)_moves.size(), linAllocator);
			for (unsigned int iter = 0; iter < m_numMoves; ++iter) {
				MTCS_Node* pSelectedNode = selection(pRoot);
				MTCS_Node* pExpandedNode = expansion(pSelectedNode, linAllocator);
				auto [reward, simPlayer] = playout(pExpandedNode, scratchMoves);
				DEBUG_ASSERT(simPlayer == pExpandedNode->m_playerTurn);
				if (pExpandedNode->m_gameState.m_winType != WinType::None) {
					DEBUG_ASSERT(simPlayer == pExpandedNode->m_pParent->m_playerTurn);
				}
				backPropagate(pExpandedNode, reward);
			}

			if (pMutex) pMutex->lock();

			DEBUG_ASSERT(pRoot->m_numChildren == sampledVisits.size());
			for (u32 j = 0; j < pRoot->m_numChildren; ++j) {
				sampledVisits[j] += pRoot->m_children[j]->m_visits;
				scores[j] += pRoot->m_children[j]->m_totalRewards;
			}

			if (pMutex) pMutex->unlock();

			pRoot->cleanup();
			linAllocator.reset();
		}
	};

	if (m_threadPool) {
		std::mutex mut;
		pMutex = &mut;
		m_threadPool->parallelize_loop(0u, m_numSampling, processRange, m_numSampling);
	}
	else {
		processRange(0u, m_numSampling);
	}
	
	// select best move among all sampled visits
	u32 bestIndex = 0;
	u32 bestVisits = 0;
	for (u32 i = 0; i < sampledVisits.size(); ++i) {
		if (sampledVisits[i] > bestVisits) {
			bestVisits = sampledVisits[i];
			bestIndex = i;
		}
	}

	return { _moves[bestIndex], scores[bestIndex] / sampledVisits[bestIndex] };
}

void  MCTS_Deterministic::initRoot(MTCS_Node* pNode, const sevenWD::Move moves[], u32 numMoves, core::LinearAllocator& linAllocator)
{
	DEBUG_ASSERT(pNode->m_numChildren == 0);
	DEBUG_ASSERT(pNode->m_numUnexploredMoves == 0);

	pNode->m_numUnexploredMoves = 0;
	
	if (numMoves > pNode->m_childrenStorage.size()) {
		pNode->m_children = new MTCS_Node*[numMoves];
	}

	for (u32 i = 0; i < numMoves; ++i) {
		sevenWD::GameController newGameState = pNode->m_gameState;
		newGameState.play(moves[i]);
		MTCS_Node* pChildNode = linAllocator.allocate<MTCS_Node>(pNode, moves[i], newGameState);
		pNode->m_children[pNode->m_numChildren++] = pChildNode;
	}
}

MTCS_Node* MCTS_Deterministic::selection(MTCS_Node* pNode)
{
	if (pNode->m_numChildren == 0 || pNode->m_numUnexploredMoves > 0) {
		return pNode;
	}
	else {
		float bestUCB = -1.0f;
		MTCS_Node* pBestChild = nullptr;
		for (u32 i = 0; i < pNode->m_numChildren; ++i) {
			MTCS_Node* pChild = pNode->m_children[i];

			// Special-case: if any child is an immediate winning move for the player who played it, take it.
			sevenWD::GameController::State st = pChild->m_gameState.m_state;
			if (st == sevenWD::GameController::State::WinPlayer0 || st == sevenWD::GameController::State::WinPlayer1) {
				u32 winner = (st == sevenWD::GameController::State::WinPlayer0) ? 0u : 1u;
				// owner is the player who made the move that produced this child
				u32 owner = pNode->m_playerTurn;
				if (owner == winner) {
					return pChild; // immediate forced win
				}
			}

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
		return pNode;
	}
	else {
		if (pNode->m_numUnexploredMoves == 0) {
			DEBUG_ASSERT(pNode->m_numChildren == 0);

			// initialize unexplored moves
			pNode->m_numUnexploredMoves = (u16)pNode->m_gameState.enumerateMoves(pNode->m_pUnexploredMoves, (u32)pNode->m_unexploredMoveStorage.size());
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

std::pair<float, u32> MCTS_Deterministic::playout(MTCS_Node* pNode, std::vector<sevenWD::Move>& scratchMoves)
{
	using namespace sevenWD;

	const u32 rootPlayer = pNode->m_playerTurn;

	if (pNode->m_gameState.m_winType != WinType::None) {
		GameController::State state = pNode->m_gameState.m_state;
		float reward = (state == GameController::State::WinPlayer0) ? (rootPlayer == 0 ? 1.0f : 0.0f) : 
			           (state == GameController::State::WinPlayer1 ? (rootPlayer == 1 ? 1.0f : 0.0f) : 0.0f);
		return { reward, rootPlayer };
	}

	GameController controller = pNode->m_gameState;
	bool end = false;
	scratchMoves.clear();
	while (!end) {
		controller.enumerateMoves(scratchMoves);
		if (scratchMoves.empty()) {
			break;
		}
		Move move = scratchMoves[m_rand() % scratchMoves.size()];
		end = controller.play(move);
	}

	// compute reward from root player perspective
	float reward = 0.0f;
	if (controller.m_state == GameController::State::WinPlayer0 && rootPlayer == 0) {
		reward = 1.0f;
	}
	else if (controller.m_state == GameController::State::WinPlayer1 && rootPlayer == 1) {
		reward = 1.0f;
	}
	else {
		reward = 0.0f;
	}

	return { reward, rootPlayer };
}

void MCTS_Deterministic::backPropagate(MTCS_Node* pNode, float reward)
{
	DEBUG_ASSERT(pNode);

	const u32 playoutPlayer = pNode->m_playerTurn;

	MTCS_Node* pCur = pNode;
	while (pCur != nullptr) {
		// increment visit count for this node
		pCur->m_visits++;

		// If node has a parent, the parent.currentPlayer is the player who played the move that produced 'pCur'.
		// Use that owner to decide reward sign: if owner == playoutPlayer use reward, else invert.
		if (pCur->m_pParent) {
			const u32 owner = pCur->m_pParent->m_playerTurn;
			const float valueForOwner = (owner == playoutPlayer) ? reward : (1.0f - reward);
			pCur->m_totalRewards += valueForOwner;
		}

		pCur = pCur->m_pParent;
	}
}