#pragma once

#pragma warning( disable : 4100 )

#include "BuildConfig.h"
#include <sstream>

struct AIInterface
{
	virtual ~AIInterface() {}
	virtual std::pair<bg::Move, float> selectMove(const bg::GameContext& _sevenWDContext, const bg::GameController& _game, const std::vector<bg::Move>& _moves, void* pThreadContext) = 0;
	virtual std::string getName() const = 0;

	virtual void* createPerThreadContext() const { return nullptr; }
	virtual void destroyPerThreadContext(void*) const {}
	virtual bool needPUCTPriors() const { return false; }
	virtual void fillPUCTPriors(void* pThreadContext, float(&puctPriors)[bg::GameController::cMaxNumMoves]) { memset(puctPriors, 0, sizeof(puctPriors)); }
};

struct RandAI : AIInterface
{
	std::pair<bg::Move, float> selectMove(const bg::GameContext& _sevenWDContext, const bg::GameController&, const std::vector<bg::Move>& _moves, void* pThreadContext) override {
		return { _moves[_sevenWDContext.rand()() % _moves.size()], 0.0f };
	}

	std::string getName() const override {
		return "RandAI";
	}
};

struct MonteCarloAI : AIInterface {
	MonteCarloAI(u32 numSimu) : m_numSimu(numSimu) {}

	std::pair<bg::Move, float> selectMove(const bg::GameContext& _sevenWDContext, const bg::GameController& _game, const std::vector<bg::Move>& _moves, void* pThreadContext) override;

	std::string getName() const {
		std::stringstream namestr;
		namestr << "MonteCarlo_" << m_numSimu;
		return namestr.str();
	}

	u32 m_numSimu;
};