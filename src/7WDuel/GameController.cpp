#include "7WDuel/GameController.h"
#include <sstream>

namespace sevenWD
{
	void GameController::enumerateMoves(std::vector<Move>& _moves) const
	{
		_moves.clear();
		if (m_state == State::DraftWonder)
		{
			u8 count = m_gameState.getNumDraftableWonders();
			for (u8 i = 0; i < count; ++i)
			{
				Move move{};
				move.playableCard = i;
				move.action = Move::DraftWonder;
				_moves.push_back(move);
			}
		}
		else if (m_state == State::Play)
		{
			for (u8 i = 0; i < m_gameState.m_numPlayableCards; ++i)
			{
				const Card& card = m_gameState.getPlayableCard(i);
				u32 cost = m_gameState.getCurrentPlayerCity().computeCost(card, m_gameState.getOtherPlayerCity());
				if (cost <= m_gameState.getCurrentPlayerCity().m_gold)
				{
					Move move = Move{ i, Move::Action::Pick };
					if (!filterMove(move))
						_moves.push_back(Move{ i, Move::Action::Pick });
				}

				Move move = Move{ i, Move::Action::Burn };
				if(!filterMove(move))
					_moves.push_back(Move{ i, Move::Action::Burn });
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
						for (u8 burnIndex = 0; burnIndex < m_gameState.m_numPlayableCards; ++burnIndex)
						{
							Move move{ burnIndex, Move::Action::BuildWonder, i };

							switch (wonder)
							{
							case Wonders::Zeus:
							{
								size_t prevMoveCount = _moves.size();
								for (u32 r = u32(ResourceType::FirstBrown); r <= u32(ResourceType::LastBrown); ++r)
								{
									if (m_gameState.getOtherPlayerCity().m_bestProductionCardId[r] != u8(-1))
									{
										move.additionalId = m_gameState.getOtherPlayerCity().m_bestProductionCardId[r];
										_moves.push_back(move);
									}
								}
								if (prevMoveCount == _moves.size())
									_moves.push_back(move);
							}
								break;

							case Wonders::CircusMaximus:
							{
								size_t prevMoveCount = _moves.size();
								for (u32 r = u32(ResourceType::FirstGrey); r <= u32(ResourceType::LastGrey); ++r)
								{
									if (m_gameState.getOtherPlayerCity().m_bestProductionCardId[r] != u8(-1))
									{
										move.additionalId = m_gameState.getOtherPlayerCity().m_bestProductionCardId[r];
										_moves.push_back(move);
									}
								}
								if (prevMoveCount == _moves.size())
									_moves.push_back(move);
							}
								break;

							case Wonders::Mausoleum:
								
							default:
								_moves.push_back(move);
								break;
							}
						}
					}
				}
			}
		}
		else if (m_state == State::PickScienceToken)
		{
			DEBUG_ASSERT(m_gameState.m_numScienceToken > 0);
			for (u8 i = 0; i < m_gameState.m_numScienceToken; ++i)
			{
				Move move{ u8(-1), Move::Action::ScienceToken };
				move.playableCard = i;

				_moves.push_back(move);
			}
		}
		else if (m_state == State::GreatLibraryToken || m_state == State::GreatLibraryTokenThenReplay)
		{
			auto unusedTokens = m_gameState.getUnusedScienceToken();
			for (u8 i = 0; i < 3; ++i)
			{
				u32 numRemainingTokens = 5 - i;
				u32 index = m_gameState.m_context->rand()() % numRemainingTokens;

				Move move{ u8(-1), Move::Action::ScienceToken };
				move.additionalId = m_gameState.m_context->getScienceToken(unusedTokens[index]).getId();
				_moves.push_back(move);

				std::swap(unusedTokens[index], unusedTokens[numRemainingTokens - 1]);
			}
		}
		else
		{
			DEBUG_ASSERT(0);
		}
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
			if (m_gameState.draftWonder(_move.playableCard))
			{
				m_state = m_gameState.isDraftingWonders() ? State::DraftWonder : State::Play;
			}
			return false;
		}
		else if (_move.action == Move::Pick)
		{
			action = m_gameState.pick(_move.playableCard);
			if (action == sevenWD::SpecialAction::TakeScienceToken && m_gameState.m_numScienceToken)
			{
				m_state = State::PickScienceToken;
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
				m_state = action == SpecialAction::Replay ? State::GreatLibraryTokenThenReplay : State::GreatLibraryToken;
				return false;
			}
		}
		else if (_move.action == Move::ScienceToken)
		{
			if(m_state == State::PickScienceToken)
				m_gameState.pickScienceToken(_move.playableCard);
			else if (m_state == State::GreatLibraryToken || m_state == State::GreatLibraryTokenThenReplay)
			{
				action = m_gameState.getCurrentPlayerCity().addCard(m_gameState.m_context->getCard(_move.additionalId), m_gameState.getOtherPlayerCity());
				if (action == SpecialAction::Nothing && m_state == State::GreatLibraryTokenThenReplay)
					action = SpecialAction::Replay;
			}
			else
				DEBUG_ASSERT(0);
		}

		if (action == sevenWD::SpecialAction::MilitaryWin || action == sevenWD::SpecialAction::ScienceWin)
		{
			m_winType = action == sevenWD::SpecialAction::MilitaryWin ? WinType::Military : WinType::Science;
			m_state = m_gameState.getCurrentPlayerTurn() == 0 ? State::WinPlayer0 : State::WinPlayer1;
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
			m_state = m_gameState.findWinner() == 0 ? State::WinPlayer0 : State::WinPlayer1;
			return true;
		}

		m_state = State::Play;
 
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
			out << "Build wonder "; ;
			m_gameState.m_context->getWonder(Wonders(m_gameState.getCurrentPlayerWonder(move.wonderIndex).getSecondaryType())).print(out);
			out << " with "; 
			return m_gameState.getPlayableCard(move.playableCard).print(out);

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