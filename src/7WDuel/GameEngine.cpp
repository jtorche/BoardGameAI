#include "7WDuel/GameEngine.h"
#include "Core/Algorithms.h"

#include <sstream>
#include <bitset>

namespace sevenWD
{
	namespace Helper
	{
		bool isReplayWonder(Wonders _wonder)
		{
			switch (_wonder)
			{
			case Wonders::HangingGarden:
			case Wonders::Atremis:
			case Wonders::Sphinx:
			case Wonders::ViaAppia:
			case Wonders::Piraeus:
				return true;
			}
			return false;
		}

		const char * toString(ResourceType _type)
		{
			switch (_type)
			{
			case ResourceType::Wood:	return "Wood";
			case ResourceType::Clay:	return "Clay";
			case ResourceType::Stone:	return "Stone";
			case ResourceType::Glass:	return "Glass";
			case ResourceType::Papyrus: return "Papyrus";
			default:
				return "None";
			}
			
			
		};

		const char* toString(CardType _type)
		{
			switch (_type)
			{
			case CardType::Blue:	return "Blue";
			case CardType::Brown:	return "Brown";
			case CardType::Grey:	return "Grey";
			case CardType::Yellow:	return "Yellow";
			case CardType::Science:	return "Science";
			case CardType::Military:return "Military";
			case CardType::Guild:	return "Guild";
			case CardType::ScienceToken: return "Token";
			case CardType::Wonder:	return "Wonder";

			default:
				return "None";
			}
		}

		const char* toString(ScienceSymbol _type)
		{
			switch (_type)
			{
			case ScienceSymbol::Wheel:		return "Wheel";
			case ScienceSymbol::Script:		return "Script";
			case ScienceSymbol::Triangle:	return "Triangle";
			case ScienceSymbol::Bowl:		return "Bowl";
			case ScienceSymbol::SolarClock:	return "SolarClock";
			case ScienceSymbol::Globe:		return "Globe";
			case ScienceSymbol::Law:		return "Law";

			default:
				return "None";
			}
		}
	}

	//----------------------------------------------------------------------------
	GameState::GameState() : m_context{ nullptr }, m_playerCity{ nullptr, nullptr }
	{
	}

	//----------------------------------------------------------------------------
	GameState::GameState(const GameContext& _context) : m_context{ &_context }, m_playerCity{ &_context, &_context }
	{
		initScienceTokens();
		initWonderDraft();
	}

	void GameState::makeDeterministic()
	{
		if (m_currentDraftRound < 2) {
			const u32 firstOutOfDraftWonderIndex = (m_currentDraftRound + 1) * 4;
			std::shuffle(m_wonderDraftPool.begin() + firstOutOfDraftWonderIndex, m_wonderDraftPool.end(), m_context->rand());
		}

		// fix a random order for draftable science tokens (Great Library)
		std::shuffle(m_scienceTokens.begin() + 5, m_scienceTokens.end(), m_context->rand());

		if (isDraftingWonders())
			initAge1Graph(true);
		if (isDraftingWonders() || m_currentAge < 1)
			initAge2Graph(true);
		if (isDraftingWonders() || m_currentAge < 2)
			initAge3Graph(true);

		// Determine cur non visible cards
		for (CardNode& node : m_graph.m_graph)
			pickCardAdnInitNode(node, m_graph);

		m_isDeterministic = true;
	}

	//----------------------------------------------------------------------------
	void GameState::initWonderDraft()
	{
		m_playerTurn = 0;
		m_currentDraftRound = 0;
		m_picksInCurrentRound = 0;

		for (PlayerCity& city : m_playerCity)
		{
			city.m_gold = 7;
			city.m_unbuildWonderCount = 0;
		}

		for (u32 i = 0; i < m_wonderDraftPool.size(); ++i)
			m_wonderDraftPool[i] = Wonders(i);

		// initial shuffle: first 4 wonders will be the round 1 draft pool
		std::shuffle(m_wonderDraftPool.begin(), m_wonderDraftPool.end(), m_context->rand());
	}

	void GameState::finishWonderDraft()
	{
		// Also initialize beginning of the game
		m_currentDraftRound = 2;
		m_playerTurn = 0;

		initAge1();
	}

	void GameState::draftWonder(u32 _draftIndex)
	{
		{
			PlayerCity& city = getCurrentPlayerCity();
			DEBUG_ASSERT(isDraftingWonders() && city.m_unbuildWonderCount < 4);

			const u32 firstPickableWonderIndex = m_currentDraftRound * 4 + m_picksInCurrentRound;
			const u32 lastPickableWonderIndex = (m_currentDraftRound + 1) * 4 - 1;

			u32 pickIndexInPool = firstPickableWonderIndex + _draftIndex;
			DEBUG_ASSERT(pickIndexInPool <= lastPickableWonderIndex);
			city.m_unbuildWonders[city.m_unbuildWonderCount++] = m_wonderDraftPool[pickIndexInPool];

			std::swap(m_wonderDraftPool[pickIndexInPool], m_wonderDraftPool[firstPickableWonderIndex]);
			m_picksInCurrentRound++;
		}

		u8 roundStarter = m_currentDraftRound; // player who started the round

		if (m_picksInCurrentRound == 1) {
			// after starter picked, other player picks next
			m_playerTurn = (roundStarter + 1) % 2;
		} else if (m_picksInCurrentRound == 2) {
			// other player's first pick: they pick again (keep same playerTurn)
		} else if (m_picksInCurrentRound == 3) {
			const u32 firstPickableWonderIndex = m_currentDraftRound * 4 + m_picksInCurrentRound;
			const u32 lastPickableWonderIndex = (m_currentDraftRound + 1) * 4 - 1;
			DEBUG_ASSERT(firstPickableWonderIndex == lastPickableWonderIndex);

			PlayerCity& starterCity = m_playerCity[roundStarter];
			Wonders remainingWonder = m_wonderDraftPool[firstPickableWonderIndex];
			starterCity.m_unbuildWonders[starterCity.m_unbuildWonderCount++] = remainingWonder;

			// complete the round
			m_currentDraftRound++;
			m_picksInCurrentRound = 0;

			if (m_currentDraftRound < 2) {
				if (!m_isDeterministic) {
					std::shuffle(m_wonderDraftPool.begin() + 4, m_wonderDraftPool.end(), m_context->rand());
				}
				m_playerTurn = 1;
			} else {
				finishWonderDraft();
			}
		}
	}

	u8 GameState::getNumDraftableWonders() const
	{
		if (!isDraftingWonders())
			return 0;

		return 4 - m_picksInCurrentRound;
	}

	Wonders GameState::getDraftableWonder(u32 _index) const
	{
		DEBUG_ASSERT(m_currentDraftRound < 2);

		const u32 firstPickableWonderIndex = m_currentDraftRound * 4 + m_picksInCurrentRound;
		return m_wonderDraftPool[firstPickableWonderIndex + _index];
	}

	//----------------------------------------------------------------------------
	const Card& GameState::getPlayableCard(u32 _index) const
	{
		DEBUG_ASSERT(_index < m_graph.m_numPlayableCards);
		u8 pickedCard = m_graph.m_playableCards[_index];
		return m_context->getCard(m_graph.m_graph[pickedCard].m_cardId);
	}

	//----------------------------------------------------------------------------
	const Card& GameState::getPlayableScienceToken(u32 _index) const
	{
		DEBUG_ASSERT(_index < m_numScienceToken);
		return m_context->getScienceToken(m_scienceTokens[_index]);
	}

	//----------------------------------------------------------------------------
	const Card& GameState::getCurrentPlayerWonder(u32 _index) const
	{
		DEBUG_ASSERT(_index < getCurrentPlayerCity().m_unbuildWonderCount);
		Wonders wonder = getCurrentPlayerCity().m_unbuildWonders[_index];
		return m_context->getWonder(wonder);
	}

	//----------------------------------------------------------------------------
	void GameState::updateMilitary(u8 military, bool hasStrategyToken)
	{
		if (military > 0)
		{
			u8 militaryBonus = hasStrategyToken ? 1 : 0;
			m_military += (m_playerTurn == 0 ? (military + militaryBonus) : -(military + militaryBonus));

			if (m_military >= 3 && !militaryToken2[0])
			{
				militaryToken2[0] = true;
				m_playerCity[1].m_gold = Helper::safeSub<u8>(m_playerCity[1].m_gold, 2);
			}
			if (m_military >= 6 && !militaryToken5[0])
			{
				militaryToken5[0] = true;
				m_playerCity[1].m_gold = Helper::safeSub<u8>(m_playerCity[1].m_gold, 5);
			}
			if (m_military <= -3 && !militaryToken2[1])
			{
				militaryToken2[1] = true;
				m_playerCity[0].m_gold = Helper::safeSub<u8>(m_playerCity[0].m_gold, 2);
			}
			if (m_military <= -6 && !militaryToken5[1])
			{
				militaryToken5[1] = true;
				m_playerCity[0].m_gold = Helper::safeSub<u8>(m_playerCity[0].m_gold, 5);
			}
		}
	}

	SpecialAction GameState::pick(u32 _playableCardIndex)
	{
		DEBUG_ASSERT(_playableCardIndex < m_graph.m_numPlayableCards);
		u8 pickedCard = m_graph.m_playableCards[_playableCardIndex];
		std::swap(m_graph.m_playableCards[_playableCardIndex], m_graph.m_playableCards[m_graph.m_numPlayableCards - 1]);
		m_graph.m_numPlayableCards--;

		unlinkNodeFromGraph(pickedCard);

		const Card& card = m_context->getCard(m_graph.m_graph[pickedCard].m_cardId);
		m_playedAgeCards[m_numPlayedAgeCards++] = card.getAgeId();

		auto& otherPlayer = m_playerCity[(m_playerTurn + 1) % 2];
		u32 cost = getCurrentPlayerCity().computeCost(card, otherPlayer);

		DEBUG_ASSERT(getCurrentPlayerCity().m_gold >= cost);
		getCurrentPlayerCity().m_gold -= u8(cost);

		if (otherPlayer.ownScienceToken(ScienceToken::Economy) && cost >= card.getGoldCost())
		{
			otherPlayer.m_gold += (u8(cost) - card.getGoldCost());
		}

		updateMilitary(card.getMilitary(), getCurrentPlayerCity().ownScienceToken(ScienceToken::Strategy));

		SpecialAction action = getCurrentPlayerCity().addCard(card, otherPlayer);

		if (abs(m_military) >= 9)
			return SpecialAction::MilitaryWin;
		else 
			return action;
	}

	void GameState::burn(u32 _playableCardIndex)
	{
		DEBUG_ASSERT(_playableCardIndex < m_graph.m_numPlayableCards);
		u8 pickedCard = m_graph.m_playableCards[_playableCardIndex];
		std::swap(m_graph.m_playableCards[_playableCardIndex], m_graph.m_playableCards[m_graph.m_numPlayableCards - 1]);
		m_graph.m_numPlayableCards--;

		unlinkNodeFromGraph(pickedCard);
		const Card& card = m_context->getCard(m_graph.m_graph[pickedCard].m_cardId);
		m_playedAgeCards[m_numPlayedAgeCards++] = card.getAgeId();

		u8 burnValue = 2 + getCurrentPlayerCity().m_numCardPerType[u32(CardType::Yellow)];
		getCurrentPlayerCity().m_gold += burnValue;
	}

	SpecialAction GameState::buildWonder(u32 _withPlayableCardIndex, u32 _wondersIndex, u8 _additionalEffect)
	{
		DEBUG_ASSERT(_withPlayableCardIndex < m_graph.m_numPlayableCards);

		u8 pickedCard = m_graph.m_playableCards[_withPlayableCardIndex];
		std::swap(m_graph.m_playableCards[_withPlayableCardIndex], m_graph.m_playableCards[m_graph.m_numPlayableCards - 1]);
		m_graph.m_numPlayableCards--;

		unlinkNodeFromGraph(pickedCard);
		const Card& card = m_context->getCard(m_graph.m_graph[pickedCard].m_cardId);
		m_playedAgeCards[m_numPlayedAgeCards++] = card.getAgeId();

		Wonders pickedWonder = getCurrentPlayerCity().m_unbuildWonders[_wondersIndex];
		std::swap(getCurrentPlayerCity().m_unbuildWonders[_wondersIndex], getCurrentPlayerCity().m_unbuildWonders[getCurrentPlayerCity().m_unbuildWonderCount - 1]);
		getCurrentPlayerCity().m_unbuildWonderCount--;

		const Card& wonder = m_context->getWonder(pickedWonder);
		auto& otherPlayer = m_playerCity[(m_playerTurn + 1) % 2];
		u32 cost = getCurrentPlayerCity().computeCost(wonder, otherPlayer);

		DEBUG_ASSERT(getCurrentPlayerCity().m_gold >= cost);
		getCurrentPlayerCity().m_gold -= u8(cost);

		if (pickedWonder == Wonders::ViaAppia)
			otherPlayer.m_gold = Helper::safeSub<u8>(otherPlayer.m_gold, 3);

		else if (_additionalEffect != u8(-1) && (pickedWonder == Wonders::Zeus || pickedWonder == Wonders::CircusMaximus))
			otherPlayer.removeCard(m_context->getCard(_additionalEffect));

		else if (_additionalEffect != u8(-1) && pickedWonder == Wonders::Mausoleum)
		{
			const Card& revivedCard = m_context->getCard(_additionalEffect);
			getCurrentPlayerCity().addCard(revivedCard, otherPlayer);
		} 
		else if (pickedWonder == Wonders::GreatLibrary)
		{
			if (!m_isDeterministic) {
				std::shuffle(m_scienceTokens.begin() + 5, m_scienceTokens.end(), m_context->rand());
			}
		}

		updateMilitary(wonder.getMilitary(), false); // strategy token do not interact with wonders

		DEBUG_ASSERT((m_playerCity[0].m_unbuildWonderCount + m_playerCity[1].m_unbuildWonderCount) > 0);

		SpecialAction action =  getCurrentPlayerCity().addCard(wonder, otherPlayer);
		if (abs(m_military) >= 9)
			return SpecialAction::MilitaryWin;
		else
			return action;
	}

	SpecialAction GameState::pickScienceToken(u32 _tokenIndex)
	{
		ScienceToken pickedToken = m_scienceTokens[_tokenIndex];
		std::swap(m_scienceTokens[_tokenIndex], m_scienceTokens[m_numScienceToken - 1]);
		m_numScienceToken--;

		return getCurrentPlayerCity().addCard(m_context->getScienceToken(pickedToken), getOtherPlayerCity());
	}

	void GameState::unlinkNodeFromGraph(u32 _nodeIndex)
	{
		DEBUG_ASSERT(m_graph.m_graph[_nodeIndex].m_child0 == CardNode::InvalidNode && m_graph.m_graph[_nodeIndex].m_child1 == CardNode::InvalidNode);

		auto removeFromParent = [&](u8 parent)
			{
				if (parent != CardNode::InvalidNode)
				{
					auto& parentNode = m_graph.m_graph[parent];
					parentNode.m_child0 = parentNode.m_child0 == _nodeIndex ? CardNode::InvalidNode : parentNode.m_child0;
					parentNode.m_child1 = parentNode.m_child1 == _nodeIndex ? CardNode::InvalidNode : parentNode.m_child1;

					if (parentNode.m_child0 == CardNode::InvalidNode && parentNode.m_child1 == CardNode::InvalidNode)
					{
						if (parentNode.m_visible == 0) {
							pickCardAdnInitNode(parentNode, m_graph);
							parentNode.m_visible = 1;
						}
						m_graph.m_playableCards[m_graph.m_numPlayableCards++] = parent;
					}
				}
			};
		removeFromParent(m_graph.m_graph[_nodeIndex].m_parent0);
		removeFromParent(m_graph.m_graph[_nodeIndex].m_parent1);
	}

	GameState::NextAge GameState::nextAge()
	{
		if (m_graph.m_numPlayableCards == 0)
		{
			if (m_currentAge == 0) {
				initAge2();
			} else if (m_currentAge == 1) {
				initAge3();
			} else if (m_currentAge == 2) {
				return NextAge::EndGame;
			}

			if (m_military < 0) // player 1 is advanced in military, player 0 to play
				m_playerTurn = 0;
			else if (m_military > 0)
				m_playerTurn = 1;
			else
				; // nothing to do, last player is the player to start the turn

			return NextAge::Next;
		}
		return NextAge::None;
	}

	u32 GameState::findWinner()
	{
		u32 vp0 = m_playerCity[0].computeVictoryPoint(m_playerCity[1]);
		u32 vp1 = m_playerCity[1].computeVictoryPoint(m_playerCity[0]);
		if (m_military >= 6)
			vp0 += 10;
		else if (m_military >= 3)
			vp0 += 5;
		else if (m_military >= 1)
			vp0 += 2;

		if (m_military <= -6)
			vp1 += 10;
		else if (m_military <= -3)
			vp1 += 5;
		else if (m_military <= -1)
			vp1 += 2;

		if (vp0 == vp1)
			return m_playerCity[0].m_numCardPerType[u32(CardType::Blue)] > m_playerCity[1].m_numCardPerType[u32(CardType::Blue)] ? 0 : 1;
		else
			return vp0 < vp1 ? 1 : 0;
	}

	void GameState::initScienceTokens()
	{
		m_scienceTokens = {
			ScienceToken::Agriculture,
			ScienceToken::Law,
			ScienceToken::Architecture,
			ScienceToken::Theology,
			ScienceToken::Strategy,

			ScienceToken::Philosophy,
			ScienceToken::TownPlanning,
			ScienceToken::Mathematics,
			ScienceToken::Masonry,
			ScienceToken::Economy 
		};

		// Use the same science token for now
		std::shuffle(std::begin(m_scienceTokens), std::end(m_scienceTokens), m_context->rand());

		m_numScienceToken = 5; // 5 first tokens are used on the board
	}

	void GameState::initAge1Graph(bool makeDeterministic)
	{
		m_graphsPerAge[0].m_age = 0;

		u32 end = genPyramidGraph(5, 0, m_graphsPerAge[0].m_graph);
		m_graphsPerAge[0].m_numPlayableCards = 0;
		for (u32 i = 0; i < 6; ++i)
			m_graphsPerAge[0].m_playableCards[m_graphsPerAge[0].m_numPlayableCards++] = u8(end - 6 + i);

		m_graphsPerAge[0].m_numAvailableGuildCards = 0;
		m_graphsPerAge[0].m_numAvailableAgeCards = m_context->getAge1CardCount();
		for (u32 i = 0; i < m_graphsPerAge[0].m_numAvailableAgeCards; ++i)
			m_graphsPerAge[0].m_availableAgeCards[i] = u8(i);

		for (CardNode& node : m_graphsPerAge[0].m_graph)
		{
			if (node.m_visible || makeDeterministic)
				pickCardAdnInitNode(node, m_graphsPerAge[0]);
		}
	}

	void GameState::initAge2Graph(bool makeDeterministic)
	{
		m_graphsPerAge[1].m_age = 1;

		u32 end = genInversePyramidGraph(6, 5, 0, m_graphsPerAge[1].m_graph);
		m_graphsPerAge[1].m_numPlayableCards = 0;
		for (u32 i = 0; i < 2; ++i)
			m_graphsPerAge[1].m_playableCards[m_graphsPerAge[1].m_numPlayableCards++] = u8(end - 2 + i);

		m_graphsPerAge[1].m_numAvailableAgeCards = m_context->getAge2CardCount();
		for (u32 i = 0; i < m_graphsPerAge[1].m_numAvailableAgeCards; ++i)
			m_graphsPerAge[1].m_availableAgeCards[i] = u8(i);
		
		for (CardNode& node : m_graphsPerAge[1].m_graph)
		{
			if (node.m_visible || makeDeterministic)
				pickCardAdnInitNode(node, m_graphsPerAge[1]);
		}
	}

	void GameState::initAge3Graph(bool makeDeterministic)
	{
		m_graphsPerAge[2].m_age = 2;

		u32 end = genPyramidGraph(3, 0, m_graphsPerAge[2].m_graph);
		const u32 connectNode0 = end;
		const u32 connectNode1 = end + 1;

		m_graphsPerAge[2].m_graph[connectNode0].m_visible = 0;
		m_graphsPerAge[2].m_graph[connectNode1].m_visible = 0;
		m_graphsPerAge[2].m_graph[connectNode0].m_isGuildCard = 0;
		m_graphsPerAge[2].m_graph[connectNode1].m_isGuildCard = 0;
		m_graphsPerAge[2].m_graph[connectNode0].m_cardId = CardNode::InvalidCardId;
		m_graphsPerAge[2].m_graph[connectNode1].m_cardId = CardNode::InvalidCardId;

		m_graphsPerAge[2].m_graph[connectNode0].m_parent0 = 5;
		m_graphsPerAge[2].m_graph[connectNode0].m_parent1 = 6;
		m_graphsPerAge[2].m_graph[5].m_child1 = connectNode0;
		m_graphsPerAge[2].m_graph[6].m_child0 = connectNode0;
		
		m_graphsPerAge[2].m_graph[connectNode1].m_parent0 = 7;
		m_graphsPerAge[2].m_graph[connectNode1].m_parent1 = 8;
		m_graphsPerAge[2].m_graph[7].m_child1 = connectNode1;
		m_graphsPerAge[2].m_graph[8].m_child0 = connectNode1;

		end = genInversePyramidGraph(4, 3, end + 2, m_graphsPerAge[2].m_graph);

		m_graphsPerAge[2].m_graph[connectNode0].m_child0 = 11;
		m_graphsPerAge[2].m_graph[connectNode0].m_child1 = 12;
		m_graphsPerAge[2].m_graph[11].m_parent1 = connectNode0;
		m_graphsPerAge[2].m_graph[12].m_parent0 = connectNode0;

		m_graphsPerAge[2].m_graph[connectNode1].m_child0 = 13;
		m_graphsPerAge[2].m_graph[connectNode1].m_child1 = 14;
		m_graphsPerAge[2].m_graph[13].m_parent1 = connectNode1;
		m_graphsPerAge[2].m_graph[14].m_parent0 = connectNode1;

		// assign randomely guild cards tag
		std::vector<u8> guildCarTag(m_graphsPerAge[2].m_graph.size(), 0);
		guildCarTag[0] = 1;
		guildCarTag[1] = 1;
		guildCarTag[2] = 1;
		std::shuffle(guildCarTag.begin(), guildCarTag.end(), m_context->rand());
		for (u32 i = 0; i < m_graphsPerAge[2].m_graph.size(); ++i)
			m_graphsPerAge[2].m_graph[i].m_isGuildCard = guildCarTag[i];

		m_graphsPerAge[2].m_numPlayableCards = 0;
		for (u32 i = 0; i < 2; ++i)
			m_graphsPerAge[2].m_playableCards[m_graphsPerAge[2].m_numPlayableCards++] = u8(end - 2 + i);

		m_graphsPerAge[2].m_numAvailableAgeCards = m_context->getAge3CardCount();
		m_graphsPerAge[2].m_numAvailableGuildCards = m_context->getGuildCardCount();

		for (u32 i = 0; i < m_graphsPerAge[2].m_numAvailableAgeCards; ++i)
			m_graphsPerAge[2].m_availableAgeCards[i] = u8(i);
		for (u32 i = 0; i < m_graphsPerAge[2].m_numAvailableGuildCards; ++i)
			m_graphsPerAge[2].m_availableGuildCards[i] = u8(i);
		
		for (CardNode& node : m_graphsPerAge[2].m_graph)
		{
			if (node.m_visible || makeDeterministic)
				pickCardAdnInitNode(node, m_graphsPerAge[2]);
		}
	}

	void GameState::initAge1()
	{
		m_currentAge = 0;

		if (!m_isDeterministic)
			initAge1Graph(false);

		m_graph = m_graphsPerAge[0];
		m_numPlayedAgeCards = 0;

	}

	void GameState::initAge2()
	{
		m_currentAge = 1;

		if (!m_isDeterministic)
			initAge2Graph(false);

		m_graph = m_graphsPerAge[1];
		m_numPlayedAgeCards = 0;
	}

	void GameState::initAge3()
	{
		m_currentAge = 2;

		if (!m_isDeterministic)
			initAge3Graph(false);

		m_graph = m_graphsPerAge[2];
		m_numPlayedAgeCards = 0;
	}

	u32 GameState::genPyramidGraph(u32 _numRow, u32 _startNodeIndex, GraphArray& graph)
	{
		u32 prevRowStartIndex = u32(-1);
		u32 curNodeIndex = _startNodeIndex;

		for (u32 row = 0; row < _numRow; ++row)
		{
			for (u32 i = 0; i < 2 + row; ++i)
			{
				graph[curNodeIndex + i].m_cardId = CardNode::InvalidCardId;
				graph[curNodeIndex + i].m_isGuildCard = 0;
				graph[curNodeIndex + i].m_visible = row % 2 == 0;
				graph[curNodeIndex + i].m_child0 = CardNode::InvalidNode;
				graph[curNodeIndex + i].m_child1 = CardNode::InvalidNode;

				if (prevRowStartIndex != u32(-1)) // not first row
				{
					if (i == 0) // first card on the row
					{
						graph[curNodeIndex + i].m_parent0 = prevRowStartIndex;
						graph[prevRowStartIndex].m_child0 = curNodeIndex + i;

						graph[curNodeIndex + i].m_parent1 = CardNode::InvalidNode;
					}
					else if (i == 1 + row) // last card on the row
					{
						graph[curNodeIndex + i].m_parent0 = prevRowStartIndex + row;
						graph[prevRowStartIndex + row].m_child1 = curNodeIndex + i;

						graph[curNodeIndex + i].m_parent1 = CardNode::InvalidNode;
					}
					else
					{
						graph[curNodeIndex + i].m_parent0 = prevRowStartIndex + i - 1;
						graph[curNodeIndex + i].m_parent1 = prevRowStartIndex + i;

						graph[prevRowStartIndex + i - 1].m_child1 = curNodeIndex + i;
						graph[prevRowStartIndex + i].m_child0 = curNodeIndex + i;
					}
				}
				else
				{
					graph[curNodeIndex + i].m_parent0 = CardNode::InvalidNode;
					graph[curNodeIndex + i].m_parent1 = CardNode::InvalidNode;
				}
			}

			prevRowStartIndex = curNodeIndex;
			curNodeIndex += 2 + row;
		}

		return curNodeIndex;
	}

	u32 GameState::genInversePyramidGraph(u32 _baseSize, u32 _numRow, u32 _startNodeIndex, GraphArray& graph)
	{
		u32 prevRowStartIndex = u32(-1);
		u32 curNodeIndex = _startNodeIndex;

		for (u32 row = 0; row < _numRow; ++row)
		{
			for (u32 i = 0; i < _baseSize - row; ++i)
			{
				graph[curNodeIndex + i].m_cardId = CardNode::InvalidCardId;
				graph[curNodeIndex + i].m_isGuildCard = 0;
				graph[curNodeIndex + i].m_visible = row % 2 == 0;
				graph[curNodeIndex + i].m_child0 = CardNode::InvalidNode;
				graph[curNodeIndex + i].m_child1 = CardNode::InvalidNode;

				if (prevRowStartIndex != u32(-1))
				{
					graph[curNodeIndex + i].m_parent0 = prevRowStartIndex + i;
					graph[curNodeIndex + i].m_parent1 = prevRowStartIndex + i + 1;

					graph[prevRowStartIndex + i].m_child1 = curNodeIndex + i;
					graph[prevRowStartIndex + 1 + i].m_child0 = curNodeIndex + i;
				}
				else
				{
					graph[curNodeIndex + i].m_parent0 = CardNode::InvalidNode;
					graph[curNodeIndex + i].m_parent1 = CardNode::InvalidNode;
				}
			}

			prevRowStartIndex = curNodeIndex;
			curNodeIndex += _baseSize - row;
		}

		return curNodeIndex;
	}

	void GameState::pickCardAdnInitNode(CardNode& _node, GraphSetup& graph)
	{
		if (_node.m_cardId == CardNode::InvalidCardId)
		{
			if (_node.m_isGuildCard)
			{
				u8 index = pickCardIndex(graph.m_availableGuildCards, graph.m_numAvailableGuildCards);
				_node.m_cardId = m_context->getGuildCard(index).getId();
			}
			else
			{
				u8 index = pickCardIndex(graph.m_availableAgeCards, graph.m_numAvailableAgeCards);
				switch (graph.m_age)
				{
				default:
					DEBUG_ASSERT(0); break;
				case 0:
					_node.m_cardId = m_context->getAge1Card(index).getId(); break;
				case 1:
					_node.m_cardId = m_context->getAge2Card(index).getId(); break;
				case 2:
					_node.m_cardId = m_context->getAge3Card(index).getId(); break;
				};
			}
		}
	}
 
	std::array<ScienceToken, 3> GameState::getGreatLibraryDraft() const
 	{
		std::array<ScienceToken, 3> tokens;
		for (u32 i = 0; i < 3; ++i)
			tokens[i] = m_scienceTokens[i + 5]; // Shuffle already done

		return tokens;
	}

	std::ostream& GameState::printPlayablCards(std::ostream& out) const
	{
		out << "Player turn : " << u32(m_playerTurn) << std::endl;
		out << "Player money : " << (u32)getCurrentPlayerCity().m_gold << std::endl;
		for (u32 i = 0; i < m_graph.m_numPlayableCards; ++i)
		{
			const Card& card = m_context->getCard(m_graph.m_graph[m_graph.m_playableCards[i]].m_cardId);
			out << i+1 << ", Cost= "<< getCurrentPlayerCity().computeCost(card, getOtherPlayerCity()) << " :";
			card.print(out);
		}

		return out;
	}

	std::ostream& GameState::printAvailableTokens(std::ostream& out) const
	{
		for (u32 i = 0; i < m_numScienceToken; ++i)
		{
			const Card& card = m_context->getScienceToken(m_scienceTokens[i]);
			out << i + 1 << ": "; card.print(out);
		}

		return out;
	}

	std::ostream& GameState::printGameState(std::ostream& out) const
	{
		out << "Military = " << int(m_military) << ", Science Token = { ";
		for (u32 i = 0; i < m_numScienceToken; ++i)
			out << m_context->getScienceToken(m_scienceTokens[i]).getName() << " ";
		
		out << "}\n";

		auto printCity = [&](const PlayerCity& _city)
		{
			out << "Gold=" << u32(_city.m_gold) << ", VP=" << u32(_city.m_victoryPoints) << ", Prod={";
			for (u32 i = 0; i < u32(ResourceType::Count); ++i)
				out << u32(_city.m_production[i]) << " ";
			out << "}, Discount={";
			for (u32 i = 0; i < u32(ResourceType::Count); ++i)
				out << u32(_city.m_resourceDiscount[i]) << " ";
			out << " ScienceToken:" << std::bitset<8>(_city.m_ownedScienceTokens) << " ";
			out << "}\n";
		};

		printCity(m_playerCity[0]);
		printCity(m_playerCity[1]);
		return out;
	}

	u32 PlayerCity::computeCost(const Card& _card, const PlayerCity& _otherPlayer) const
	{
		if (_card.m_chainIn != ChainingSymbol::None && m_chainingSymbols & (1u << u32(_card.m_chainIn)))
			return 0;

		std::array<u8, u32(RT::Count)> goldCostPerResource = { 2,2,2,2,2 }; // base cost
		for (u32 i = 0; i < u32(RT::Count); ++i)
		{
			goldCostPerResource[i] += _otherPlayer.m_production[i];
			goldCostPerResource[i] = m_resourceDiscount[i] ? 1 : goldCostPerResource[i];
		}

		std::array<u8, u32(RT::Count)> cardResourceCost = _card.m_cost;
		bool empty = true;
		for (u32 i = 0; i < u32(RT::Count); ++i)
		{
			cardResourceCost[i] = Helper::safeSub(cardResourceCost[i], m_production[i]);
			empty &= cardResourceCost[i] == 0;
		}
		
		if (!empty)
		{
			ResourceType normalResources[] = { RT::Wood, RT::Clay, RT::Stone };
			ResourceType rareResources[] = { RT::Glass, RT::Papyrus };
			std::sort(std::begin(normalResources), std::end(normalResources), [&](ResourceType _1, ResourceType _2) { return goldCostPerResource[u32(_1)] > goldCostPerResource[u32(_2)]; });
			std::sort(std::begin(rareResources), std::end(rareResources), [&](ResourceType _1, ResourceType _2) { return goldCostPerResource[u32(_1)] > goldCostPerResource[u32(_2)]; });
			

			if (ownScienceToken(ScienceToken::Masonry) && _card.m_type == CardType::Blue ||
				ownScienceToken(ScienceToken::Architecture) && _card.m_type == CardType::Wonder)
			{
				ResourceType allResources[] = { RT::Wood, RT::Clay, RT::Stone, RT::Glass, RT::Papyrus };
				std::sort(std::begin(allResources), std::end(allResources), [&](ResourceType _1, ResourceType _2) { return goldCostPerResource[u32(_1)] > goldCostPerResource[u32(_2)]; });

				u32 freeResources = 2;
				for (ResourceType r : allResources)
				{
					while (freeResources > 0 && cardResourceCost[u32(r)] > 0)
					{
						cardResourceCost[u32(r)]--;
						freeResources--;
					}
				}
			}

			// first spend normal resource
			for (u32 i = 0; i < m_weakProduction.first; ++i)
			{
				for (ResourceType r : normalResources)
				{
					if (cardResourceCost[u32(r)] > 0)
					{
						cardResourceCost[u32(r)]--;
						break;
					}
				}
			}

			// then same for rare resource
			for (u32 i = 0; i < m_weakProduction.second; ++i)
			{
				for (ResourceType r : rareResources)
				{
					if (cardResourceCost[u32(r)] > 0)
					{
						cardResourceCost[u32(r)]--;
						break;
					}
				}
			}

			u32 finalCost = 0;
			for (u32 i = 0; i < u32(RT::Count); ++i)
				finalCost += cardResourceCost[i] * goldCostPerResource[i];

			return finalCost + _card.m_goldCost;
		}
	 return _card.m_goldCost;
	}

	SpecialAction PlayerCity::addCard(const Card& _card, const PlayerCity& _otherCity)
	{
		SpecialAction action = SpecialAction::Nothing;

		if (_card.m_chainIn != ChainingSymbol::None && m_chainingSymbols & (1u << u32(_card.m_chainIn)) && ownScienceToken(ScienceToken::TownPlanning))
			m_gold += 4;

		m_chainingSymbols |= (1u << u32(_card.m_chainOut));

		if (_card.m_goldPerNumberOfCardColorTypeCard)
			m_gold += m_numCardPerType[_card.m_secondaryType] * _card.m_goldReward;
		else if(_card.m_type == CardType::Guild && _card.m_secondaryType < u32(CardType::Count))
			m_gold += std::max(m_numCardPerType[_card.m_secondaryType], _otherCity.m_numCardPerType[_card.m_secondaryType]) * _card.m_goldReward;
		else
			m_gold += _card.m_goldReward;

		if (_card.m_type == CardType::Brown || _card.m_type == CardType::Grey)
		{
			for (u32 type = 0; type < u32(ResourceType::Count); ++type)
			{
				if (_card.m_production[type] > 0)
				{
					if (m_bestProductionCardId[type] == u8(-1) ||
						_card.m_production[type] > m_context->getCard(m_bestProductionCardId[type]).m_production[type])
					{
						m_bestProductionCardId[type] = _card.getId();
					}
				}
			}
		}

		m_numCardPerType[u32(_card.m_type)]++;
		if (_card.m_type != CardType::Guild) {
			m_victoryPoints += _card.m_victoryPoints;
		}

		for (u32 i = 0; i < u32(ResourceType::Count); ++i)
		{
			if (_card.m_isResourceDiscount)
				m_resourceDiscount[i] |= _card.m_production[i] > 0;
			else if (_card.m_isWeakProduction); 
				// handled differently
			else
				m_production[i] += _card.m_production[i];
		}

		if (_card.m_isWeakProduction)
		{
			// DEBUG_ASSERT(_card.m_production[u32(RT::Wood)] == _card.m_production[u32(RT::Clay)] && 
			// 			 _card.m_production[u32(RT::Wood)] == _card.m_production[u32(RT::Stone]);
			// DEBUG_ASSERT(_card.m_production[u32(RT::Glass)] == _card.m_production[u32(RT::Papyrus)]);

			m_weakProduction.first += _card.m_production[u32(RT::Wood)];
			m_weakProduction.second += _card.m_production[u32(RT::Glass)];
		}

		switch (_card.m_type)
		{
		case CardType::Science:
			m_ownedScienceSymbol[u32(_card.m_science)]++;
			DEBUG_ASSERT(m_ownedScienceSymbol[u32(_card.m_science)] < 3);
			if (m_ownedScienceSymbol[u32(_card.m_science)] == 2)
				action = SpecialAction::TakeScienceToken;
			else
				m_numScienceSymbols++;
			break;

		case CardType::Guild:
			m_ownedGuildCards |= (1u << _card.m_secondaryType);
			break;

		case CardType::ScienceToken:
			if(ScienceToken(_card.m_secondaryType) == ScienceToken::Mathematics)
				m_victoryPoints += 3 * u8(core::countBits(m_ownedScienceTokens));

			if (ScienceToken(_card.m_secondaryType) == ScienceToken::Law)
			{
				m_ownedScienceSymbol[u32(ScienceSymbol::Law)]++;
				m_numScienceSymbols++;
			}

			m_ownedScienceTokens |= 1u << _card.m_secondaryType;
			if (ownScienceToken(ScienceToken::Mathematics))
				m_victoryPoints += 3;
			break;

		case CardType::Wonder:
			if (Helper::isReplayWonder(Wonders(_card.m_secondaryType)) || ownScienceToken(ScienceToken::Theology))
				action = SpecialAction::Replay;
			break;
		}
		
		if (m_numScienceSymbols == 6)
			return SpecialAction::ScienceWin;

		return action;
	}

	void PlayerCity::removeCard(const Card& _card)
	{
		DEBUG_ASSERT(_card.getType() == CardType::Brown || _card.getType() == CardType::Grey);
		DEBUG_ASSERT(_card.m_chainIn == ChainingSymbol::None && _card.m_chainOut == ChainingSymbol::None);

		for(u32 i=0 ; i < u32(ResourceType::Count) ; ++i)
			m_production[i] -= _card.m_production[i];
	}

	u32 PlayerCity::computeVictoryPoint(const PlayerCity& _otherCity) const
	{
		u32 goldVP = m_gold / 3;
		if (m_ownedGuildCards & (1 << u32(CardType::Count)))
			goldVP *= 2;

		u32 guildVP = 0;
		for (const Card& card : m_context->getAllGuildCards())
		{
			if (card.getSecondaryType() < u32(CardType::Count) && m_ownedGuildCards & (1 << card.getSecondaryType()))
			{
				u32 numCards = std::max(m_numCardPerType[card.getSecondaryType()], _otherCity.m_numCardPerType[card.getSecondaryType()]);
				guildVP += card.m_victoryPoints * numCards;
			}
		}

		return m_victoryPoints + goldVP + guildVP;
	}

	DiscardedCards::DiscardedCards()
	{
		for (u8& x : scienceCards)
			x = u8(-1);
		for (u8& x : guildCards)
		 x = u8(-1);
	}

	void DiscardedCards::add(const GameContext& _context, const Card& _card)
	{
		if (_card.getMilitary() > 0)
		{
			if (militaryCard == u8(-1) || _card.getMilitary() > _context.getCard(militaryCard).getMilitary())
				militaryCard = _card.getId();
		}

		if (_card.m_victoryPoints > 0)
		{
			if (bestVictoryPoint == u8(-1) || _card.m_victoryPoints > _context.getCard(bestVictoryPoint).m_victoryPoints)
				bestVictoryPoint = _card.getId();
		}

		if (_card.getType() == CardType::Science)
		{
			u32 scienceIndex = u32(_card.m_science);
			if (scienceCards[scienceIndex] != u8(-1))
			{
				if(_card.m_victoryPoints > _context.getCard(bestVictoryPoint).m_victoryPoints)
					scienceCards[scienceIndex] = _card.getId();
			}
			else
				scienceCards[scienceIndex] = _card.getId();
		}

		if (_card.getType() == CardType::Guild)
			guildCards[numGuildCards++] = _card.getId();
	}

	template<typename T>
	u32 GameState::fillTensorData(T* _data, u32 _mainPlayer) const
	{
		u32 i = 0;
		_data[i++] = (T)m_numTurnPlayed;
		_data[i++] = (T)m_currentAge;
		_data[i++] = (T)(_mainPlayer == 0 ? m_military : -m_military);
		_data[i++] = (T)militaryToken2[_mainPlayer];
		_data[i++] = (T)militaryToken5[_mainPlayer];
		_data[i++] = (T)militaryToken2[(_mainPlayer + 1) % 2];
		_data[i++] = (T)militaryToken5[(_mainPlayer + 1) % 2];

		for (u32 j = 0; j < u32(ScienceToken::Count); ++j) {
			_data[i + j] = 0;
		}
		for (u32 j = 0; j < m_numScienceToken; ++j)
			_data[i + u32(m_scienceTokens[j])] = 1;

		i += u32(ScienceToken::Count);

		const PlayerCity& myCity = m_playerCity[_mainPlayer];
		const PlayerCity& opponentCity = m_playerCity[(_mainPlayer + 1) % 2];
		for (u8 j = 0; j < u8(Wonders::Count); ++j)
		{
			if (std::find(myCity.m_unbuildWonders.begin(), myCity.m_unbuildWonders.end(), (Wonders)j) != myCity.m_unbuildWonders.end())
				_data[i] = 1;
			else if (std::find(opponentCity.m_unbuildWonders.begin(), opponentCity.m_unbuildWonders.end(), (Wonders)j) != opponentCity.m_unbuildWonders.end())
				_data[i] = -1;
			else
				_data[i] = 0;
			
			i++;
		}

		auto fillCity = [&](const PlayerCity& _city)
		{
			for(u8 j=0 ; j<u8(ChainingSymbol::Count) ; ++j)
				_data[i++] = (T)((_city.m_chainingSymbols & (1 << j)) > 0 ? 1 : 0);

			for (size_t j=0 ; i<m_context->getAllGuildCards().size() ; ++j)
				_data[i++] = (T)((_city.m_ownedGuildCards & (1 << j)) > 0 ? 1 : 0);

			for (u8 j = 0; j < u8(ScienceToken::Count); ++j)
				_data[i++] = (T)((_city.m_ownedScienceTokens & (1 << j)) > 0 ? 1 : 0);

			_data[i++] = _city.m_numScienceSymbols;
			for (u8 j = 0; j < u8(ScienceSymbol::Count); ++j)
				_data[i++] = (T)_city.m_ownedScienceSymbol[j];

			_data[i++] = (T)_city.m_gold;
			_data[i++] = (T)_city.m_victoryPoints;
			
			for (u8 j = 0; j < u8(CardType::Count); ++j)
			{
				_data[i++] = (T)_city.m_numCardPerType[j];
				_data[i++] = (T)(_city.m_resourceDiscount[j] ? 1 : 0);
			}

			for (u8 j = 0; j < u8(ResourceType::Count); ++j)
			{
				_data[i++] = (T)_city.m_production[j];
				_data[i++] = (T)_city.m_bestProductionCardId[j];
			}

			_data[i++] = (T)_city.m_weakProduction.first;
			_data[i++] = (T)_city.m_weakProduction.second;

			T numPlayAgainWonders = 0;
			for (u8 j = 0; j < _city.m_unbuildWonderCount; ++i) {
				if (Helper::isReplayWonder(_city.m_unbuildWonders[i]))
					numPlayAgainWonders += 1;
			}
			_data[i++] = numPlayAgainWonders;
		};

		fillCity(myCity);
		fillCity(opponentCity);

		return i;
	}

	template u32 GameState::fillTensorData<float>(float* _data, u32 _mainPlayer) const;
	template u32 GameState::fillTensorData<int16_t>(int16_t* _data, u32 _mainPlayer) const;

	template<typename T>
	void GameState::fillExtraTensorData(T* _data) const
	{
		for (u32 i = 0; i < GameContext::MaxCardsPerAge; ++i) {
			_data[i] = 0;
			_data[i+GameContext::MaxCardsPerAge] = -1;
		}

		// Cards that have already been picked/burn by a player
		for (u32 i = 0; i < m_numPlayedAgeCards; ++i)
			_data[m_playedAgeCards[i]] = -1;

		// Cards that have not been revealed in the graph
		// active graph usage
		for (u32 i = 0; i < m_graph.m_numAvailableAgeCards; ++i)
			_data[m_graph.m_availableAgeCards[i]] = 1;

		// go through the graph, process visible cards
		std::array<bool, 20> visitedNodes = {};

		u32 numNodeToExplore = m_graph.m_numPlayableCards;
		std::array <u8, 6> nodesToExplore = m_graph.m_playableCards;
		u32 nextNumNodeToExplore = 0;
		std::array <u8, 6> nextNodesToExplore = {};
		
		u32 depth = 0;
		while (numNodeToExplore > 0)
		{
			for (u32 i = 0; i < numNodeToExplore; ++i)
			{
				if (m_graph.m_graph[nodesToExplore[i]].m_visible)
				{
					u8 id = m_context->getCard(m_graph.m_graph[nodesToExplore[i]].m_cardId).getAgeId();
					_data[id] = 2; // card visible in the graph
					_data[GameContext::MaxCardsPerAge + id] = (T)(depth + 1);
				}

				auto gatherParent = [&](u8 parent) {
					if (parent != CardNode::InvalidNode)
					{
						if (!visitedNodes[parent])
						{
							visitedNodes[parent] = true;
							nextNodesToExplore[nextNumNodeToExplore++] = parent;
						}
					}
				};
				gatherParent(m_graph.m_graph[nodesToExplore[i]].m_parent0);
				gatherParent(m_graph.m_graph[nodesToExplore[i]].m_parent1);
				
			}

			numNodeToExplore = nextNumNodeToExplore;
			nodesToExplore = nextNodesToExplore;
			nextNumNodeToExplore = 0;
			depth++;
		}
	}

	template void GameState::fillExtraTensorData<float>(float* _data) const;
	template void GameState::fillExtraTensorData<int16_t>(int16_t* _data) const;
}
