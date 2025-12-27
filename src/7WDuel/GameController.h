#pragma once

#include "GameEngine.h"

namespace sevenWD
{
	struct Move
	{
		enum Action : u8 { Pick, Burn, BuildWonder, ScienceToken, DraftWonder, Count };
		u8 playableCard;
		Action action;
		u8 wonderIndex = u8(-1);
		u8 additionalId = u8(-1);

		u32 compteMoveFixedIndex() const
		{
			DEBUG_ASSERT(playableCard < 6 || playableCard == u8(-1));
			switch (action)
			{
			case Action::ScienceToken:
				return 0;
			case Action::Pick:
			case Action::DraftWonder:
				return playableCard;
			case Action::Burn:
				return 6 + playableCard;
			case Action::BuildWonder:
				return 12 + wonderIndex * 6 + playableCard;
			default:
				DEBUG_ASSERT(0);
				return 0;
			}
		}
	};

	enum class WinType
	{
		None,
		Civil,
		Military,
		Science
	};

	struct GameController
	{
		static constexpr u32 cMaxNumMoves = 36; // 6 pickable cards * (pick + burn + buildWonders * 4) = 6*(1+1+4)=36

		using State = GameState::State;

		GameController(const GameContext& _context, bool autoDraftWonders = false) : m_gameState(_context)
		{
			m_gameState.m_state = m_gameState.isDraftingWonders() ? State::DraftWonder : State::Play;
			if (autoDraftWonders) {
				while (m_gameState.isDraftingWonders()) {
					m_gameState.draftWonder(0);
				}
				m_gameState.m_state = State::Play;
			}
		}

		template<typename Fun>
		void enumerateMoves(Fun&& _fun) const;

		void enumerateMoves(std::vector<Move>&) const;
		u32 enumerateMoves(Move outMoves[], u32 bufferSize) const;
		bool play(Move _move);

		bool filterMove(Move _move) const;
	
		GameState m_gameState;
		WinType m_winType = WinType::None;

// #define RECORD_GAME_HISTORY
#if defined(RECORD_GAME_HISTORY)
		std::vector<std::string> m_gameHistory;
#endif

		std::ostream& printMove(std::ostream& out, Move move) const;
	};
}