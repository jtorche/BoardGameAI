#include "GuessNumber.h"

namespace guessNum
{
	void GameState::enumerateMoves(std::vector<Move>& _moves) const
	{
		_moves.clear();
		enumerateMoves([&](const Move& move) {
			_moves.push_back(move);
			});
	}

	u32 GameState::enumerateMoves(Move outMoves[], u32 bufferSize) const
	{
		u32 count = 0;
		enumerateMoves([&](const Move& move) {
			if (count < bufferSize)
				outMoves[count] = move;
			count++;
			});
		return count;
	}

	bool GameState::play(Move _move)
	{
		m_guesses[m_playerTurn].push_back(_move.m_guess);

		if (m_guesses[m_playerTurn].back() == m_secretNumber[m_playerTurn]) {
			m_winType = (m_playerTurn == 0) ? WinType::Player0 : WinType::Player1;
		}
		else if (m_guesses[m_playerTurn].size() >= cMaxTurnAllowed) {
			m_winType = (m_playerTurn == 0) ? WinType::Player1 : WinType::Player0;
		}
		m_playerTurn = (m_playerTurn + 1) % 2;
		return m_winType != WinType::None;
	}

	std::vector<u8> GameState::serializeGameState() const {
		std::vector<u8> blob;

		auto writeInt = [&blob](int value) {
			const u8* ptr = reinterpret_cast<const u8*>(&value);
			blob.insert(blob.end(), ptr, ptr + sizeof(int));
		};

		for (int i = 0; i < 2; ++i) {
			writeInt(m_secretNumber[i]);

			int guessCount = static_cast<int>(m_guesses[i].size());
			writeInt(guessCount);
			for (int guess : m_guesses[i]) {
				writeInt(guess);
			}
		}

		return blob;
	}

	bool GameState::deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob)
	{
		size_t offset = 0;

		auto readInt = [&_blob, &offset](int& value) -> bool {
			if (offset + sizeof(int) > _blob.size()) {
				return false;
			}

			std::memcpy(&value, _blob.data() + offset, sizeof(int));
			offset += sizeof(int);
			return true;
		};

		for (int i = 0; i < 2; ++i) {
			int secretNumber = 0;
			if (!readInt(secretNumber)) {
				return false;
			}

			int guessCount = 0;
			if (!readInt(guessCount)) {
				return false;
			}

			if (guessCount < 0) {
				return false;
			}

			std::vector<int> guesses;
			guesses.reserve(static_cast<size_t>(guessCount));

			for (int j = 0; j < guessCount; ++j) {
				int guess = 0;
				if (!readInt(guess)) {
					return false;
				}
				guesses.push_back(guess);
			}

			m_secretNumber[i] = secretNumber;
			m_guesses[i] = std::move(guesses);
		}

		return true;
	}
}