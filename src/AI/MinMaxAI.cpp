#include "AI/MinMaxAI.h"
#include <future>

#include "Core/thread_pool.h"

namespace sevenWD
{
#ifdef _DEBUG
    thread_pool s_threadPool(1);
#else
    thread_pool s_threadPool;
#endif

    MinMaxAI::MinMaxAI(const MinMaxAIHeuristic* pHeuristic, u32 _maxDepth, bool _monothread) : m_pHeuristic(pHeuristic), m_maxDepth{ _maxDepth }, m_monothread{ _monothread }
    {

    }

    std::pair<Move, float> MinMaxAI::selectMove(const GameContext&, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext)
    {
        std::vector<std::future<float>> asyncScores(_moves.size());
        std::atomic<float> b = 1.0f;

        auto evalPosLambda = [&](const Move& _move)
        {
            GameController newGameState = _game;

            u32 curPlayer = _game.m_gameState.getCurrentPlayerTurn();
            float score;
            if (newGameState.play(_move))
            {
                if (curPlayer == 0 && newGameState.m_state == GameController::State::WinPlayer0)
                    score = 1.0f;
                else if (curPlayer == 1 && newGameState.m_state == GameController::State::WinPlayer1)
                    score = 1.0f;
                else
                    score = 0.0f;
            }
            else
                score = evalRec(curPlayer, newGameState, 1, { 0.0f, b }, pThreadContext);

            b.store(std::min(b.load(), score));
            return score;
        };

        std::transform(_moves.begin(), _moves.end(), asyncScores.begin(),
            [&](const Move& move) -> std::future<float>
            {
                if (m_monothread)
                {
                    std::promise<float> scorePromise;
                    scorePromise.set_value(evalPosLambda(move));
                    return scorePromise.get_future();
                }
                else
                    return s_threadPool.submit([&]() -> float { return evalPosLambda(move); });
            });

        std::vector<float> scores(asyncScores.size());
        std::transform(asyncScores.begin(), asyncScores.end(), scores.begin(),
            [&](std::future<float>& _score)
            {
                return _score.get();
            });

        auto bestIt = std::max_element(scores.begin(), scores.end());
        u32 bestScoreIndex = (u32)std::distance(scores.begin(), bestIt);
        return { _moves[bestScoreIndex], scores[bestScoreIndex] };
    }

	//-------------------------------------------------------------------------------------------------
	float MinMaxAI::evalRec(u32 _maxPlayer, const GameController& _gameState, u32 _depth, vec2 _a_b, void* pThreadContext)
	{
        bool isMaxPlayerTurn = _gameState.m_gameState.getCurrentPlayerTurn() == _maxPlayer;

        if (_depth >= m_maxDepth || m_stopAI)
        {
            n_numLeafEpxlored++;
            return m_pHeuristic->computeScore(_gameState.m_gameState, _maxPlayer, nullptr);
        }

        std::vector<Move> moves;
        _gameState.enumerateMoves(moves);

        m_numMoves += moves.size();
        m_numNodeExplored++;

        // The "maxPlayer" supposes the opponent minimize his score, reverse for the "minPlayer"
        float score = isMaxPlayerTurn ? 1.0f : 0.0f;

        auto minmax = [isMaxPlayerTurn](float s1, float s2)
        {
            return isMaxPlayerTurn ? std::min(s1, s2) : std::max(s1, s2);
        };

        for (size_t i = 0; i < moves.size(); ++i)
        {
            GameController newGameState = _gameState;
            bool gameTerminated = newGameState.play(moves[i]);

            float moveScore;
            if (gameTerminated)
            {
                if (_maxPlayer == 0 && newGameState.m_state == GameController::State::WinPlayer0)
                    moveScore = 1.0f;
                else if(_maxPlayer == 1 && newGameState.m_state == GameController::State::WinPlayer1)
                    moveScore = 1.0f;
                else
                    moveScore = 0.0f;
            }
            else
                moveScore = evalRec(_maxPlayer, newGameState, _depth + 1, _a_b, pThreadContext);

            score = minmax(moveScore, score);

            if (isMaxPlayerTurn)
            {
                if (score <= _a_b.x)
                    return score;
                _a_b.y = std::min(_a_b.y, score);
            }
            else
            {
                if (score >= _a_b.y)
                    return score;
                _a_b.x = std::max(_a_b.x, score);
            }
        }

        return score;
	}

    //-------------------------------------------------------------------------------------------------
    // float MinMaxAI::computeScore(const GameController& _gameState, u32 _maxPlayer) const
    // {
    //     const PlayerCity& player = _gameState.m_gameState.getPlayerCity(_maxPlayer);
    //     const PlayerCity& opponent = _gameState.m_gameState.getPlayerCity((_maxPlayer + 1) % 2);
    // 
    //     return float(player.computeVictoryPoint(opponent)) - float(opponent.computeVictoryPoint(player));
    // }
}
