#pragma once

#pragma warning( disable : 4100 )

#include "7WDuel/GameController.h"
#include <sstream>

namespace sevenWD
{
	struct AIInterface 
	{
		virtual ~AIInterface() {}
		virtual std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext) = 0;
		virtual std::string getName() const = 0;

		virtual void* createPerThreadContext() const { return nullptr; }
		virtual void destroyPerThreadContext(void*) const { }
	};

	struct RandAI : AIInterface
	{
		std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController&, const std::vector<Move>& _moves, void* pThreadContext) override {
			return { _moves[_sevenWDContext.rand()() % _moves.size()], 0.0f };
		}

		std::string getName() const override {
			return "RandAI";
		}
	};

	struct NoBurnAI : AIInterface
	{
		std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController&, const std::vector<Move>& _moves, void* pThreadContext) override;

		std::string getName() const override {
			return "NoBurnAI";
		}
	};

	struct PriorityAI : AIInterface
	{
		PriorityAI(bool focusMilitary, bool focusScience) : m_focusMilitary(focusMilitary), m_focusScience(focusScience) {}
		std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController&, const std::vector<Move>& _moves, void* pThreadContext) override;

		std::string getName() const override {
			if (m_focusMilitary) {
				return "PriorityMilitaryAI";
			}
			if (m_focusScience) {
				return "PriorityScienceAI";
			}
			return "PriorityAI";
		}

		bool m_focusMilitary;
		bool m_focusScience;
	};

	struct MixAI : AIInterface {
		unsigned int m_precentage;
		std::unique_ptr<AIInterface> m_AIs[2];

		template<typename AI0, typename AI1>
		MixAI(AI0* pAI0, AI1* pAI1, unsigned int _precentage) : m_precentage(_precentage)
		{
			m_AIs[0] = std::unique_ptr<AI0>(pAI0);
			m_AIs[1] = std::unique_ptr<AI1>(pAI1);
		}

		std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext) override {
			return m_precentage >= _sevenWDContext.rand()() % 100 ? m_AIs[0]->selectMove(_sevenWDContext, _game, _moves, pThreadContext) : m_AIs[1]->selectMove(_sevenWDContext, _game, _moves, pThreadContext);
		}

		std::string getName() const {
			std::ostringstream stringStream;
			stringStream << "MixAI(" << m_AIs[0]->getName() << "," << m_AIs[1]->getName() << ")";
			return stringStream.str();
		}
	};

	struct MonteCarloAI : AIInterface {
		MonteCarloAI(u32 numSimu) : m_numSimu(numSimu) {}

		std::pair<Move, float> selectMove(const GameContext& _sevenWDContext, const GameController& _game, const std::vector<Move>& _moves, void* pThreadContext) override;

		std::string getName() const {
			std::stringstream namestr;
			namestr << "MonteCarlo_" << m_numSimu;
			return namestr.str();
		}

		u32 m_numSimu;
	};
}