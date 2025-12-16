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
		GameController(const GameContext& _context) : m_gameState(_context)
		{
			m_state = m_gameState.isDraftingWonders() ? State::DraftWonder : State::Play;
		}

		template<typename Fun>
		void enumerateMoves(Fun&& _fun) const;

		void enumerateMoves(std::vector<Move>&) const;
		u32 enumerateMoves(Move outMoves[], u32 bufferSize) const;
		bool play(Move _move);

		bool filterMove(Move _move) const;
	
		GameState m_gameState;
		WinType m_winType = WinType::None;

		enum class State
		{
			DraftWonder,
			Play,
			PickScienceToken,
			GreatLibraryToken,
			GreatLibraryTokenThenReplay,
			WinPlayer0,
			WinPlayer1
		};
		State m_state = State::Play;

// #define RECORD_GAME_HISTORY
#if defined(RECORD_GAME_HISTORY)
		std::vector<std::string> m_gameHistory;
#endif

		std::ostream& printMove(std::ostream& out, Move move) const;
	};
}