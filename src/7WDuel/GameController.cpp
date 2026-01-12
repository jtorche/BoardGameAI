#include "7WDuel/GameController.h"
#include <sstream>

namespace sevenWD
{
	template<typename Fun>
	void GameController::enumerateMoves(Fun&& _fun) const
	{
		if (m_gameState.m_state == State::DraftWonder)
		{
			u8 count = m_gameState.getNumDraftableWonders();
			for (u8 i = 0; i < count; ++i)
			{
				Move move{};
				move.playableCard = i;
				move.action = Move::DraftWonder;
				_fun(move);
			}
		}
		else if (m_gameState.m_state == State::Play)
		{
			for (u8 i = 0; i < m_gameState.getNumPlayableCards(); ++i)
			{
				const Card& card = m_gameState.getPlayableCard(i);
				u32 cost = m_gameState.getCurrentPlayerCity().computeCost(card, m_gameState.getOtherPlayerCity());
				if (cost <= m_gameState.getCurrentPlayerCity().m_gold)
				{
					Move move = Move{ i, Move::Action::Pick };
					if (!filterMove(move))
						_fun(Move{ i, Move::Action::Pick });
				}

				Move move = Move{ i, Move::Action::Burn };
				if (!filterMove(move))
					_fun(Move{ i, Move::Action::Burn });
			}

			u8 totalUnbuilt = m_gameState.m_playerCity[0].m_unbuildWonderCount + m_gameState.m_playerCity[1].m_unbuildWonderCount;
			u8 builtSoFar = 8 - totalUnbuilt;
			if (builtSoFar < 7) // 7 wonders max
			{
				for (u8 i = 0; i < m_gameState.getCurrentPlayerCity().m_unbuildWonderCount; ++i)
				{
					Wonders wonder = m_gameState.getCurrentPlayerCity().m_unbuildWonders[i];
					const Card& wonderCard = m_gameState.m_context->getWonder(wonder);

					u32 cost = m_gameState.getCurrentPlayerCity().computeCost(wonderCard, m_gameState.getOtherPlayerCity());
					if (cost <= m_gameState.getCurrentPlayerCity().m_gold)
					{
						for (u8 burnIndex = 0; burnIndex < m_gameState.getNumPlayableCards(); ++burnIndex)
						{
							Move move{ burnIndex, Move::Action::BuildWonder, i };

							switch (wonder)
							{
							case Wonders::Zeus:
							{
								bool canDelete = false;
								for (u32 r = u32(ResourceType::FirstBrown); r <= u32(ResourceType::LastBrown); ++r)
								{
									if (m_gameState.getOtherPlayerCity().m_bestProductionCardId[r] != u8(-1))
									{
										move.additionalId = m_gameState.getOtherPlayerCity().m_bestProductionCardId[r];
										_fun(move);
										canDelete = true;
									}
								}
								if (!canDelete)
									_fun(move);
							}
							break;

							case Wonders::CircusMaximus:
							{
								bool canDelete = false;
								for (u32 r = u32(ResourceType::FirstGrey); r <= u32(ResourceType::LastGrey); ++r)
								{
									if (m_gameState.getOtherPlayerCity().m_bestProductionCardId[r] != u8(-1))
									{
										move.additionalId = m_gameState.getOtherPlayerCity().m_bestProductionCardId[r];
										_fun(move);
										canDelete = true;
									}
								}
								if (!canDelete)
									_fun(move);
							}
							break;

							case Wonders::Mausoleum:
							{
								std::vector<u8> revivableCards;
								m_gameState.m_discardedCards.getRevivableCards(revivableCards);
								
								if (revivableCards.empty())
								{
									// No cards to revive, still can build the wonder
									_fun(move);
								}
								else
								{
									for (u8 cardId : revivableCards)
									{
										move.additionalId = cardId;
										_fun(move);
									}
								}
							}
							break;

							default:
								_fun(move);
								break;
							}
						}
					}
				}
			}
		}
		else if (m_gameState.m_state == State::PickScienceToken)
		{
			DEBUG_ASSERT(m_gameState.m_numScienceToken > 0);
			for (u8 i = 0; i < m_gameState.m_numScienceToken; ++i)
			{
				Move move{ u8(-1), Move::Action::ScienceToken };
				move.playableCard = i;

				_fun(move);
			}
		}
		else if (m_gameState.m_state == State::GreatLibraryToken || m_gameState.m_state == State::GreatLibraryTokenThenReplay)
		{
			auto tokenDraft = m_gameState.getGreatLibraryDraft();
			for (u8 i = 0; i < 3; ++i)
			{
				Move move{ i, Move::Action::ScienceToken };
				_fun(move);
			}
		}
		else
		{
			DEBUG_ASSERT(0);
		}
	}

	void GameController::enumerateMoves(std::vector<Move>& _moves) const
	{
		_moves.clear();
		enumerateMoves([&](const Move& move) {
			_moves.push_back(move);
		});
	}

	u32 GameController::enumerateMoves(Move outMoves[], u32 bufferSize) const
	{
		u32 count = 0;
		enumerateMoves([&](const Move& move) {
			if (count < bufferSize)
				outMoves[count] = move;
			count++;
		});
		return count;
	}

	bool GameController::play(Move _move)
	{
#if defined(RECORD_GAME_HISTORY)
		std::stringstream strstream;
		strstream << "Player " << m_gameState.getCurrentPlayerTurn() << " : ";
		printMove(strstream, _move);
		m_gameHistory.push_back(strstream.str());
#endif

		SpecialAction action = sevenWD::SpecialAction::Nothing;

		if (_move.action == Move::DraftWonder)
		{
			m_gameState.draftWonder(_move.playableCard);
			m_gameState.m_state = m_gameState.isDraftingWonders() ? State::DraftWonder : State::Play;
			return false;
		}
		else if (_move.action == Move::Pick)
		{
			action = m_gameState.pick(_move.playableCard);
			if (action == sevenWD::SpecialAction::TakeScienceToken && m_gameState.m_numScienceToken)
			{
				m_gameState.m_state = State::PickScienceToken;
				return false;
			}
		}
		else if (_move.action == Move::Burn)
			m_gameState.burn(_move.playableCard);
		else if (_move.action == Move::BuildWonder)
		{
			Wonders wonder = Wonders(m_gameState.getCurrentPlayerWonder(_move.wonderIndex).getSecondaryType());
			action = m_gameState.buildWonder(_move.playableCard, _move.wonderIndex, _move.additionalId);
			if (wonder == Wonders::GreatLibrary)
			{
				m_gameState.m_state = action == SpecialAction::Replay ? State::GreatLibraryTokenThenReplay : State::GreatLibraryToken;
				return false;
			}
		}
		else if (_move.action == Move::ScienceToken)
		{
			if(m_gameState.m_state == State::PickScienceToken)
				action = m_gameState.pickScienceToken(_move.playableCard, false);
			else if (m_gameState.m_state == State::GreatLibraryToken || m_gameState.m_state == State::GreatLibraryTokenThenReplay)
			{
				action = m_gameState.pickScienceToken(_move.playableCard, true);
				if (action == SpecialAction::Nothing && m_gameState.m_state == State::GreatLibraryTokenThenReplay)
					action = SpecialAction::Replay;
			}
			else
				DEBUG_ASSERT(0);
		}

		if (action == sevenWD::SpecialAction::MilitaryWin || action == sevenWD::SpecialAction::ScienceWin)
		{
			m_winType = action == sevenWD::SpecialAction::MilitaryWin ? WinType::Military : WinType::Science;
			m_gameState.m_state = m_gameState.getCurrentPlayerTurn() == 0 ? State::WinPlayer0 : State::WinPlayer1;
			return true;
		}

		sevenWD::GameState::NextAge ageState = m_gameState.nextAge();
		if (ageState == sevenWD::GameState::NextAge::None)
		{
			if (action != sevenWD::SpecialAction::Replay)
				m_gameState.nextPlayer();
		}
		else if (ageState == sevenWD::GameState::NextAge::EndGame)
		{
			m_winType = WinType::Civil;
			m_gameState.m_state = m_gameState.findWinner() == 0 ? State::WinPlayer0 : State::WinPlayer1;
			return true;
		}

		m_gameState.m_state = State::Play;
 
		return false;
	}

	bool GameController::filterMove(Move /*_move*/) const
	{
		//if (_move.action == Move::Burn)
		//{
		//	if (m_gameState.getCurrentPlayerCity().m_gold > 10)
		//		return true;
		//}

		return false;
	}

	std::ostream& GameController::printMove(std::ostream& out, Move move) const
	{
		switch (move.action) {
		case Move::Pick:
			out << "Pick ";
			return m_gameState.getPlayableCard(move.playableCard).print(out);
		case Move::Burn:
			out << "Burn ";
			return m_gameState.getPlayableCard(move.playableCard).print(out);

		case Move::BuildWonder:
		{
			out << "Build wonder ";
			Wonders wonderType = Wonders(m_gameState.getCurrentPlayerWonder(move.wonderIndex).getSecondaryType());
			m_gameState.m_context->getWonder(wonderType).print(out);
			out << " with "; 
			m_gameState.getPlayableCard(move.playableCard).print(out);
			
			if (move.additionalId != u8(-1))
			{
				if (wonderType == Wonders::Zeus || wonderType == Wonders::CircusMaximus)
				{
					out << " destroying ";
					m_gameState.m_context->getCard(move.additionalId).print(out);
				}
				else if (wonderType == Wonders::Mausoleum)
				{
					out << " reviving ";
					m_gameState.m_context->getCard(move.additionalId).print(out);
				}
			}
			return out;
		}

		case Move::ScienceToken:
			out << "Pick science token " << move.playableCard; 
			return out;

		case Move::DraftWonder:
			out << "Draft wonder option " << int(move.playableCard);
			return out;
		}

		return out;
	}
}