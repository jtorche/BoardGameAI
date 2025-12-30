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
				if (game.m_gameState.m_state == GameState::State::WinPlayer0 && rootPlayer == 0) {
					s = 1.0;
				}
				else if (game.m_gameState.m_state == GameState::State::WinPlayer1 && rootPlayer == 1) {
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
MCTS_Deterministic::MCTS_Deterministic(u32 numMoves, u32 numGameState, bool mt) : BaseNetworkAI("MCTS_Deterministic", {}), m_numMoves(numMoves), m_numSampling(numGameState)
{
	if (mt) {
		m_threadPool = new thread_pool(std::thread::hardware_concurrency());
	}
}

std::pair<sevenWD::Move, float> MCTS_Deterministic::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	float maxDepthAvg;
	std::vector<u32> sampledVisits(_moves.size(), 0);
	std::vector<float> scores(_moves.size(), 0);
	std::mutex* pMutex = nullptr;

	auto processRange = [&](u32 start, u32 end)
	{
		core::LinearAllocator linAllocator(8 * 1024 * 1024);
		std::vector<Move> scratchMoves;

		for (u32 i = start; i < end; ++i) {
			u32 maxDepth = 0;
			MTCS_Node* pRoot = linAllocator.allocate<MTCS_Node>((MTCS_Node*)nullptr, Move{}, _game);
			pRoot->m_gameState.m_gameState.makeDeterministic();
			initRoot(pRoot, _moves.data(), (u32)_moves.size(), linAllocator);
			for (unsigned int iter = 0; iter < m_numMoves; ++iter) {
				u32 depth = 0;
				MTCS_Node* pSelectedNode = selection(pRoot, depth);
				MTCS_Node* pExpandedNode = expansion(pSelectedNode, linAllocator);
				auto [reward, simPlayer] = playout(pExpandedNode, scratchMoves, pThreadContext);
				DEBUG_ASSERT(simPlayer == pExpandedNode->m_playerTurn);
				if (pExpandedNode->m_gameState.m_winType != WinType::None) {
					DEBUG_ASSERT(simPlayer == pExpandedNode->m_pParent->m_playerTurn);
				}
				backPropagate(pExpandedNode, reward);
				maxDepth = std::max(depth, maxDepth);
			}

			if (pMutex) pMutex->lock();

			DEBUG_ASSERT(pRoot->m_numChildren == sampledVisits.size());
			maxDepthAvg += (float)maxDepth;
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

	maxDepthAvg = maxDepthAvg / m_numSampling;
	
	// select best move among all sampled visits
	u32 bestIndex = 0;
	u32 bestVisits = 0;
	for (u32 i = 0; i < sampledVisits.size(); ++i) {
		scores[i] /= sampledVisits[i];
		if (sampledVisits[i] > bestVisits) {
			bestVisits = sampledVisits[i];
			bestIndex = i;
		}
	}

	return { _moves[bestIndex], scores[bestIndex] };
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

MTCS_Node* MCTS_Deterministic::selection(MTCS_Node* pNode, u32& depth)
{
	depth++;
	if (pNode->m_numChildren == 0 || pNode->m_numUnexploredMoves > 0) {
		return pNode;
	}
	else {
		float bestUCB = -1.0f;
		MTCS_Node* pBestChild = nullptr;
		for (u32 i = 0; i < pNode->m_numChildren; ++i) {
			MTCS_Node* pChild = pNode->m_children[i];

			// Special-case: if any child is an immediate winning move for the player who played it, take it.
			sevenWD::GameController::State st = pChild->m_gameState.m_gameState.m_state;
			if (st == sevenWD::GameState::State::WinPlayer0 || st == sevenWD::GameState::State::WinPlayer1) {
				u32 winner = (st == sevenWD::GameState::State::WinPlayer0) ? 0u : 1u;
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
		return selection(pBestChild, depth);
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
			pNode->m_numUnexploredMoves = (u16)pNode->m_gameState.enumerateMoves(pNode->m_pMoves, (u32)pNode->m_moveStorage.size());
			DEBUG_ASSERT(pNode->m_numUnexploredMoves > 0);

			if (pNode->m_numUnexploredMoves > pNode->m_moveStorage.size()) {
				pNode->m_pMoves = new sevenWD::Move[pNode->m_numUnexploredMoves];
				pNode->m_children = new MTCS_Node*[pNode->m_numUnexploredMoves];
				pNode->m_gameState.enumerateMoves(pNode->m_pMoves, pNode->m_numUnexploredMoves);
			}
		}

		// expand
		u32 moveIndex = m_rand() % pNode->m_numUnexploredMoves;
		sevenWD::Move move = pNode->m_pMoves[moveIndex];
		// remove move from unexplored moves
		pNode->m_pMoves[moveIndex] = pNode->m_pMoves[pNode->m_numUnexploredMoves - 1];
		--pNode->m_numUnexploredMoves;
		// create child node
		sevenWD::GameController newGameState = pNode->m_gameState;
		newGameState.play(move);
		MTCS_Node* pChildNode = linAllocator.allocate<MTCS_Node>(pNode, move, newGameState);
		pNode->m_children[pNode->m_numChildren++] = pChildNode;
		return pChildNode;
	}
}

std::pair<float, u32> MCTS_Deterministic::playout(MTCS_Node* pNode, std::vector<sevenWD::Move>& scratchMoves, void* pThreadContext)
{
	using namespace sevenWD;

	const u32 rootPlayer = pNode->m_playerTurn;

	if (pNode->m_gameState.m_winType != WinType::None) {
		GameController::State state = pNode->m_gameState.m_gameState.m_state;
		float reward = (state == GameState::State::WinPlayer0) ? (rootPlayer == 0 ? 1.0f : 0.0f) : 
			           (state == GameState::State::WinPlayer1 ? (rootPlayer == 1 ? 1.0f : 0.0f) : 0.0f);
		return { reward, rootPlayer };
	}

	if (m_heuristic == Heuristic_UseDNN) {
		return { pNode->m_nnHeuristic, rootPlayer };
	}

	GameController controller = pNode->m_gameState;
	bool end = false;
	scratchMoves.clear();
	while (!end) {
		controller.enumerateMoves(scratchMoves);
		if (scratchMoves.empty()) {
			break;
		}

		bool hasMoved = false;
		if (m_heuristic == Heuristic_NoBurnRollout) {
			// choose randomely among non-burn moves if possible
			u32 numNonBurnMoves = 0;
			for (u32 i = 0; i < scratchMoves.size(); ++i) {
				if (scratchMoves[i].action != Move::Action::Burn) {
					++numNonBurnMoves;
				}
			}
			if (numNonBurnMoves > 0) {
				u32 index = m_rand() % numNonBurnMoves;
				for (u32 i = 0; i < scratchMoves.size(); ++i) {
					if (scratchMoves[i].action != Move::Action::Burn) {
						if (index == 0) {
							Move move = scratchMoves[i];
							end = controller.play(move);
							hasMoved = true;
							break;
						}
						--index;
					}
				}
			}
		}

		if (!hasMoved) {
			Move move = scratchMoves[m_rand() % scratchMoves.size()];
			end = controller.play(move);
		}
	}

	// compute reward from root player perspective
	float reward = 0.0f;
	if (controller.m_gameState.m_state == GameState::State::WinPlayer0 && rootPlayer == 0) {
		reward = 1.0f;
	}
	else if (controller.m_gameState.m_state == GameState::State::WinPlayer1 && rootPlayer == 1) {
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

// --------------------------------------------------- //
// --------------------- MCTS_Zero ------------------- //
// --------------------------------------------------- //
MCTS_Zero::MCTS_Zero(u32 numMoves, u32 numGameState, bool mt) : BaseNetworkAI("MCTS_Zero", {}), m_numMoves(numMoves), m_numSampling(numGameState), m_useNNHeuristic(false)
{
	if (mt) {
		m_threadPool = new thread_pool(std::thread::hardware_concurrency());
	}
	C = 5.0f; // when no PUCT priors are used, a higher C value is beneficial
}

std::pair<sevenWD::Move, float> MCTS_Zero::selectMove(const sevenWD::GameContext& _sevenWDContext, const sevenWD::GameController& _game, const std::vector<sevenWD::Move>& _moves, void* pThreadContext)
{
	using namespace sevenWD;

	float maxDepthAvg = 0;
	std::vector<u32> sampledVisits(_moves.size(), 0);
	std::vector<float> scores(_moves.size(), 0);
	float puctPriors[GameController::cMaxNumMoves] = { 0 };
	std::mutex* pMutex = nullptr;

	auto processRange = [&](u32 start, u32 end)
	{
			core::LinearAllocator linAllocator(8 * 1024 * 1024);
			std::vector<Move> scratchMoves;

			for (u32 i = start; i < end; ++i) {
				u32 maxDepth = 0;
				MTCS_Node* pRoot = linAllocator.allocate<MTCS_Node>((MTCS_Node*)nullptr, Move{}, _game);
				pRoot->m_gameState.m_gameState.makeDeterministic();
				initRoot(pRoot, _moves.data(), (u32)_moves.size(), linAllocator, pThreadContext);
				for (unsigned int iter = 0; iter < m_numMoves; ++iter) {
					u32 depth = 0;
					MTCS_Node* pSelectedNode = selection(pRoot, depth, linAllocator, pThreadContext);
					auto [reward, simPlayer] = playout(pSelectedNode, scratchMoves, pThreadContext);
					DEBUG_ASSERT(simPlayer == pSelectedNode->m_playerTurn);
					if (pSelectedNode->m_gameState.m_winType != WinType::None) {
						DEBUG_ASSERT(simPlayer == pSelectedNode->m_pParent->m_playerTurn);
					}
					backPropagate(pSelectedNode, reward);
					maxDepth = std::max(depth, maxDepth);
				}

				if (pMutex) pMutex->lock();

				DEBUG_ASSERT(pRoot->m_numChildren == sampledVisits.size());
				maxDepthAvg += (float)maxDepth;
				for (u32 j = 0; j < pRoot->m_numChildren; ++j) {
					sampledVisits[j] += pRoot->m_children[j]->m_visits;
					scores[j] += pRoot->m_children[j]->m_totalRewards;

					u32 moveFixedIndex = pRoot->m_children[j]->m_move_from_parent.computeMoveFixedIndex();
					puctPriors[moveFixedIndex] += (float)pRoot->m_children[j]->m_visits / pRoot->m_visits;
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

	maxDepthAvg = maxDepthAvg / m_numSampling;

	// select best move among all sampled visits
	u32 bestIndex = 0;
	u32 bestVisits = 0;
	for (u32 i = 0; i < sampledVisits.size(); ++i) {
		scores[i] /= sampledVisits[i];
		puctPriors[_moves[i].computeMoveFixedIndex()] /= m_numSampling;
		if (sampledVisits[i] > bestVisits) {
			bestVisits = sampledVisits[i];
			bestIndex = i;
		}
	}

	if (pThreadContext) {
		ThreadContext* pTC = (ThreadContext*)pThreadContext;
		memcpy(pTC->m_puctPriors, puctPriors, sizeof(puctPriors));
	}

	return { _moves[bestIndex], scores[bestIndex] };
}

void MCTS_Zero::computeNNInference(MTCS_Node* pNode, void* pContext) const
{
	const sevenWD::GameState& state = pNode->m_gameState.m_gameState;
	u32 curPlayer = pNode->m_playerTurn;

	u8 age = (u8)state.getCurrentAge();
	age = age == u8(-1) ? 0 : age;
	auto& network = m_network[age];

	const u32 tensorSize = sevenWD::GameState::TensorSize + (network->m_extraTensorData ? sevenWD::GameState::ExtraTensorSize : 0);

#if defined(USE_TINY_DNN)
	tiny_dnn::vec_t buffer(tensorSize);
	state.fillTensorData(buffer.data(), curPlayer);
	if (network->m_extraTensorData)
		state.fillExtraTensorData(buffer.data() + sevenWD::GameState::TensorSize);

	ThreadContext* pThreadContext = (ThreadContext*)pContext;
	DEBUG_ASSERT(pThreadContext == nullptr || pThreadContext->m_pThis == this);
	tiny_dnn::vec_t output = network->forward(buffer, pThreadContext, age);

	float curPlayerWinProbability = output[0];
	pNode->m_nnHeuristic = curPlayerWinProbability;
	memcpy(pNode->m_puctPriors, &output[1], sizeof(float) * sevenWD::GameController::cMaxNumMoves);
#else
	DEBUG_ASSERT(false);
#endif
}

void MCTS_Zero::initPUCTPriors(MTCS_Node* pNode, void* pThreadContext, const sevenWD::Move moves[], u32 numMoves) const
{
	if (m_useNNHeuristic && !pNode->m_gameState.m_gameState.isDraftingWonders()) {
		computeNNInference(pNode, pThreadContext);

	} else {
		for (u32 i = 0; i < sevenWD::GameController::cMaxNumMoves; ++i) {
			pNode->m_puctPriors[i] = 1.f / sevenWD::GameController::cMaxNumMoves;
		}
	}

	// Mask non valid moves and normalize the output probability
	float mask[sevenWD::GameController::cMaxNumMoves] = { 0 };
	for (u32 i = 0; i < numMoves; ++i) {
		mask[moves[i].computeMoveFixedIndex()] = 1.0;
	}

	float sum = cEpsilon;
	for (u32 i = 0; i < sevenWD::GameController::cMaxNumMoves; ++i) {
		sum += pNode->m_puctPriors[i] * mask[i];
	}

	float invSum = 1.0f / sum;
	for (u32 i = 0; i < sevenWD::GameController::cMaxNumMoves; ++i) {
		pNode->m_puctPriors[i] = pNode->m_puctPriors[i] * mask[i] * invSum;
	}
}

static std::vector<float> sample_dirichlet(int A, float alpha, std::mt19937& rng) 
{
	std::gamma_distribution<float> gamma(alpha, 1.0f);
	std::vector<float> noise(A);
	float sum = 0.0f;
	for (int a = 0; a < A; ++a) {
		noise[a] = gamma(rng);
		sum += noise[a];
	}
	for (int a = 0; a < A; ++a) {
		noise[a] /= sum;
	}
	return noise;
}

void MCTS_Zero::initRoot(MTCS_Node* pNode, const sevenWD::Move moves[], u32 numMoves, core::LinearAllocator& linAllocator, void* pThreadContext)
{
	DEBUG_ASSERT(pNode->m_numChildren == 0);
	if (numMoves > pNode->m_childrenStorage.size()) {
		pNode->m_pMoves = new sevenWD::Move[numMoves];
		pNode->m_children = new MTCS_Node * [numMoves];
	}

	for (u32 i = 0; i < numMoves; ++i) {
		sevenWD::GameController newGameState = pNode->m_gameState;
		newGameState.play(moves[i]);
		MTCS_Node* pChildNode = linAllocator.allocate<MTCS_Node>(pNode, moves[i], newGameState);
		pNode->m_pMoves[i] = moves[i];
		pNode->m_children[i] = pChildNode;
	}

	pNode->m_numChildren = (u8)numMoves;
	initPUCTPriors(pNode, pThreadContext, moves, numMoves);

	// dirichlet noise to improve exploration for NN prior training
	if (m_useNNHeuristic) {
		float epsilon = 0.25f;
		float alpha = 0.3f;
		std::vector<float> noise = sample_dirichlet(numMoves, alpha, m_rand);
		for (u32 a = 0; a < numMoves; ++a) {
			u32 id = moves[a].computeMoveFixedIndex();
			pNode->m_puctPriors[id] = (1.0f - epsilon) * pNode->m_puctPriors[id] + epsilon * noise[a];
		}
	}
}

MTCS_Node* MCTS_Zero::selection(MTCS_Node* pNode, u32& depth, core::LinearAllocator& linAllocator, void* pThreadContext)
{
	depth++;

	if (pNode->m_gameState.m_winType != sevenWD::WinType::None) {
		return pNode;
	}

	if (pNode->m_numChildren == 0) {
		// First time we see this node, initialize it
		u32 numMoves = (u16)pNode->m_gameState.enumerateMoves(pNode->m_pMoves, (u32)pNode->m_moveStorage.size());
		if (numMoves > pNode->m_moveStorage.size()) {
			pNode->m_pMoves = new sevenWD::Move[numMoves];
			pNode->m_children = new MTCS_Node*[numMoves];
			pNode->m_gameState.enumerateMoves(pNode->m_pMoves, numMoves);
		}

		for (u32 i = 0; i < numMoves; ++i) {
			// Lazy allocation, children are created when selected
			pNode->m_children[pNode->m_numChildren++] = nullptr;
		}

		initPUCTPriors(pNode, pThreadContext, pNode->m_pMoves, numMoves);
		return pNode;
	}

	float bestPUCT = -1.0f;
	u32 bestChildIdx = u32(-1);

	// Parent visit count for exploration term
	const float parentVisits = static_cast<float>(pNode->m_visits) + 1.0f;

	for (u32 i = 0; i < pNode->m_numChildren; ++i) {
		MTCS_Node* pChild = pNode->m_children[i];

		if (pChild) {
			// Early-out: if this child is an immediate win for the player who played it, take it.
			sevenWD::GameController::State st = pChild->m_gameState.m_gameState.m_state;
			if (st == sevenWD::GameState::State::WinPlayer0 || st == sevenWD::GameState::State::WinPlayer1) {
				u32 winner = (st == sevenWD::GameState::State::WinPlayer0) ? 0u : 1u;
				u32 owner = pNode->m_playerTurn; // player that chose this move
				if (owner == winner) {
					return pChild;
				}
			}
		}

		const float childVisits = pChild ? static_cast<float>(pChild->m_visits) : 0.0f;
		// Q(s,a): average value from this node's perspective, make sure to assign 50/50 probability to unvisited nodes
	    float qValue = (childVisits == 0) ? 0.5f : pChild->m_totalRewards / childVisits;

		// P(s,a) from priors
		const u32 moveIndex = pNode->m_pMoves[i].computeMoveFixedIndex();
		const float prior = pNode->m_puctPriors[moveIndex];

		// U(s,a): exploration term
		const float uValue = C * prior * sqrtf(parentVisits) / (1.0f + childVisits);

		const float puct = qValue + uValue;

		if (puct > bestPUCT) {
			bestPUCT = puct;
			bestChildIdx = i;
		}
	}

	if (pNode->m_children[bestChildIdx] == nullptr) {
		sevenWD::GameController newGameState = pNode->m_gameState;
		newGameState.play(pNode->m_pMoves[bestChildIdx]);
		MTCS_Node* pChildNode = linAllocator.allocate<MTCS_Node>(pNode, pNode->m_pMoves[bestChildIdx], newGameState);
		pNode->m_children[bestChildIdx] = pChildNode;
	}
	return selection(pNode->m_children[bestChildIdx], depth, linAllocator, pThreadContext);
}

std::pair<float, u32> MCTS_Zero::playout(MTCS_Node* pNode, std::vector<sevenWD::Move>& scratchMoves, void* pThreadContext)
{
	using namespace sevenWD;

	const u32 rootPlayer = pNode->m_playerTurn;

	if (pNode->m_gameState.m_winType != WinType::None) {
		GameController::State state = pNode->m_gameState.m_gameState.m_state;
		float reward = (state == GameState::State::WinPlayer0) ? (rootPlayer == 0 ? 1.0f : 0.0f) :
			(state == GameState::State::WinPlayer1 ? (rootPlayer == 1 ? 1.0f : 0.0f) : 0.0f);
		return { reward, rootPlayer };
	}

	if (m_useNNHeuristic) {
	    return { pNode->m_nnHeuristic, rootPlayer };
	}

	GameController controller = pNode->m_gameState;
	bool end = false;
	scratchMoves.clear();
	while (!end) {
		controller.enumerateMoves(scratchMoves);
		Move move = scratchMoves[m_rand() % scratchMoves.size()];
		end = controller.play(move);
	}

	// compute reward from root player perspective
	float reward = 0.0f;
	if (controller.m_gameState.m_state == GameState::State::WinPlayer0 && rootPlayer == 0) {
		reward = 1.0f;
	}
	else if (controller.m_gameState.m_state == GameState::State::WinPlayer1 && rootPlayer == 1) {
		reward = 1.0f;
	}
	else {
		reward = 0.0f;
	}

	return { reward, rootPlayer };
}

void MCTS_Zero::backPropagate(MTCS_Node* pNode, float reward)
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