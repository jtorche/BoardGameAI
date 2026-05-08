#pragma once

#pragma warning( disable : 4100 )

#if defined(BUILD_FOR_7WDUEL)
#include "7WDuel/GameController.h"
namespace bg {
	static constexpr u32 cNumNetworks = 3; // 3 ages
	using ::sevenWD::GameContext;
	using ::sevenWD::GameController;
	using ::sevenWD::Move;
	using ::sevenWD::WinType;
	static const char* s_bgPrefix = "SevenWD";

	struct SerializableGameState {
		::sevenWD::GameState m_state;

		SerializableGameState() = default;
		SerializableGameState(const GameContext& _context) : m_state(_context) {}
		SerializableGameState(const GameController& _state) : m_state(_state.m_gameState) {}
		u32 getCurrentPlayerTurn() const { return m_state.getCurrentPlayerTurn(); }

		u32 fillTensorData(float* _data, u32 _mainPlayer) const {
			return m_state.fillTensorData(_data, _mainPlayer);
		}
		void fillExtraTensorData(float* _data, u32 _mainPlayer) const {
			m_state.fillExtraTensorData(_data, _mainPlayer);
		}

		std::vector<u8> serializeGameState() const {
			return sevenWD::Helper::serializeGameState(m_state);
		}
		bool deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob) {
			return sevenWD::Helper::deserializeGameState(_context, _blob, m_state);
		}
	};
}
#elif defined(BUILD_FOR_GUESSNUMBER)
#include "GuessNumber/GuessNumber.h"
namespace bg {
	static constexpr u32 cNumNetworks = 1;
	using ::guessNum::GameContext;
	using GameController = ::guessNum::GameState;
	using ::guessNum::Move;
	using ::guessNum::WinType;
	static const char* s_bgPrefix = "GuessNum";

	struct SerializableGameState {
		::guessNum::GameState m_state;

		SerializableGameState() = default;
		SerializableGameState(const GameContext& _context) : m_state(_context) {}
		SerializableGameState(const GameController& _state) : m_state(_state) {}
		u32 getCurrentPlayerTurn() const { return m_state.getCurrentPlayerTurn(); }

		u32 fillTensorData(float* _data, u32 _mainPlayer) const {
			return m_state.fillTensorData(_data, _mainPlayer);
		}
		void fillExtraTensorData(float* _data, u32 _mainPlayer) const {
			m_state.fillExtraTensorData(_data, _mainPlayer);
		}

		std::vector<u8> serializeGameState() const {
			return m_state.serializeGameState();
		}
		bool deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob) {
			return m_state.deserializeGameState(_context, _blob);
		}
	};
}
#endif