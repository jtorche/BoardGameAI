#pragma once
#include "Core/Common.h"
#include "Core/type.h"

#include <random>

namespace guessNum
{
	struct GameContext {
		GameContext(unsigned int _seed = 42) : m_rand(_seed) {}
		std::default_random_engine& rand() const { return m_rand; }

		mutable std::default_random_engine m_rand;
	};

	struct Move {
		int m_guess = 0;

		int computeMoveFixedIndex(const GameContext& ctx) const {
			return m_guess - 1;
		}
	};

	enum class WinType {
		None=0,
		Player0,
		Player1,
		Count,
	};

	class GameState {
	public:
		static constexpr u32 cNumberRange = 100; 
		static constexpr u32 cMaxTurnAllowed = 10;

		static constexpr u32 cMaxNumMoves = cNumberRange;

		const GameContext* getContextPtr() const { return nullptr; }

		GameState() : m_secretNumber{0,0} {}
		GameState(const GameContext& _context)
		{
			m_secretNumber[0] = 1 + (_context.rand()() % cNumberRange);
			m_secretNumber[1] = 1 + (_context.rand()() % cNumberRange);
		}

		void makeDeterministic() {
		}

		template<typename Fun>
		void enumerateMoves(Fun&& _fun) const {
			for (int i = 0; i < cNumberRange; ++i) {
				_fun(Move{i + 1});
			}
		}

		void enumerateMoves(std::vector<Move>&) const;
		u32 enumerateMoves(Move outMoves[], u32 bufferSize) const;
		bool play(Move _move);

		u32 getCurrentPlayerTurn() const { return m_playerTurn; }

		u32 getWinner() const {
			switch (m_winType) {
			case WinType::Player0:
				return 0;
			case WinType::Player1:
				return 1;
			default:
				return UINT_MAX;
			}
		}

		static const u32 TensorSize = cMaxTurnAllowed * 4;
		static const u32 ExtraTensorSize = 0;

		template<typename T>
		u32 fillTensorData(T* _data, u32 _mainPlayer) const {
			u32 index = 0;
			for (int i = 0; i < cMaxTurnAllowed; ++i) {
				_data[index++] = (T)m_guesses[0][i];
				_data[index++] = (T)m_guesses[1][i];
			}
			for (int i = 0; i < cMaxTurnAllowed; ++i) {
				_data[index++] = (T)(m_guesses[0][i] < m_secretNumber[0] ? -1 : (m_guesses[0][i] > m_secretNumber[0] ? 1 : 0));
				_data[index++] = (T)(m_guesses[1][i] < m_secretNumber[1] ? -1 : (m_guesses[1][i] > m_secretNumber[1] ? 1 : 0));
			}
			return index;
		}
		template<typename T>
		void fillExtraTensorData(T* _data, u32 _mainPlayer) const {
		}

		std::vector<u8> serializeGameState() const;
		bool deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob);

		u8 getNetId() const {
			return 0;
		}

		WinType m_winType = WinType::None;

	private:
		int m_playerTurn = 0;
		int m_secretNumber[2];
		std::vector<int> m_guesses[2];
	};
}