#pragma once

#include "GameEngine.h"

namespace sevenWD
{
	struct Move
	{
		enum Action : u8 { Pick, Burn, BuildWonder, BuildMausoleum, ScienceToken, DraftWonder };
		u8 playableCard;
		Action action;
		u8 wonderIndex = u8(-1);
		u8 additionalId = u8(-1);

		u32 computeMoveFixedIndex(const GameContext& ctx) const
		{
			DEBUG_ASSERT(playableCard < 6 || playableCard == u8(-1));
			switch (action)
			{
			case Action::ScienceToken:
			case Action::Pick:
			case Action::DraftWonder:
				return playableCard;
			case Action::Burn:
				return 6 + playableCard;
			case Action::BuildWonder:
			case Action::BuildMausoleum:
			{
				bool isReviveScienceOrMilitaryWithMausoleum = false;
				if (action == Action::BuildMausoleum && additionalId != u8(-1)) {
					CardType reviveType = ctx.getCard(additionalId).getType();
					isReviveScienceOrMilitaryWithMausoleum = (reviveType == CardType::Science || reviveType == CardType::Military);
				}
				if (isReviveScienceOrMilitaryWithMausoleum) {
					return 36 + playableCard; // assign a unique slot for this special case to help network discover strong play pattern (to achieve tricky science/military win)
				} else {
					return 12 + wonderIndex * 6 + playableCard;
				}
			}
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
		Science,
		Count,
	};

	struct GameController
	{
		static constexpr u32 cMaxNumMoves = 42; // 6 pickable cards * (pick + burn + (buildWonders * 4) + SpecialMausoleum) = 6*(1+1+4+1)=42

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

		const GameContext* getContextPtr() const { return m_gameState.m_context; }

		void makeDeterministic() {
			m_gameState.makeDeterministic();
		}

		template<typename Fun>
		void enumerateMoves(Fun&& _fun) const;

		void enumerateMoves(std::vector<Move>&) const;
		u32 enumerateMoves(Move outMoves[], u32 bufferSize) const;
		bool play(Move _move);

		u32 getCurrentPlayerTurn() const { return m_gameState.getCurrentPlayerTurn(); }
		u32 getWinner() const {
			switch (m_gameState.m_state) {
			case GameState::State::WinPlayer0:
				return 0;
			case GameState::State::WinPlayer1:
				return 1;
			default:
				return UINT_MAX;
			}
		}

		static const u32 TensorSize = GameState::TensorSize;
		static const u32 ExtraTensorSize = GameState::ExtraTensorSize;

		template<typename T>
		u32 fillTensorData(T* _data, u32 _mainPlayer) const {
			return m_gameState.fillTensorData(_data, _mainPlayer);
		}
		template<typename T>
		void fillExtraTensorData(T* _data, u32 _mainPlayer) const {
			m_gameState.fillExtraTensorData(_data, _mainPlayer);
		}

		u8 getNetId() const { 
			u8 age = (u8)m_gameState.getCurrentAge();
			return age == u8(-1) ? 0 : age;
		}

		GameState m_gameState;
		WinType m_winType = WinType::None;

// #define RECORD_GAME_HISTORY
#if defined(RECORD_GAME_HISTORY)
		std::vector<std::string> m_gameHistory;
#endif

		std::ostream& printMove(std::ostream& out, Move move) const;
	};
}