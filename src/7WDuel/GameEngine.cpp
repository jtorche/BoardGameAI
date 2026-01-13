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

		if (!isDraftingWonders()) {
			// Determine cur non visible cards
			for (CardNode& node : m_graph.m_graph)
				pickCardAdnInitNode(node, m_graph);
		}
		m_isDeterministic = true;
	}

#ifdef _DEBUG
	void GameState::updatePlaybleCardPtrDebug()
	{
		for (u32 i = 0; i < 6; ++i) {
			if (i < m_graph.m_numPlayableCards) {
				m_playableCardsPtr[i] = &getPlayableCard(i);
			}
			else {
				m_playableCardsPtr[i] = nullptr;
			}
		}
	}
#else
	void GameState::updatePlaybleCardPtrDebug() {}
#endif

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
	const Card& GameState::getPlayableScienceToken(u32 _index, bool isGreatLibraryDraft) const
	{
		DEBUG_ASSERT(_index < (isGreatLibraryDraft ? 3u : m_numScienceToken));
		_index = isGreatLibraryDraft ? _index + 5 : _index;
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

		// Track the discarded card for potential Mausoleum revival
		m_discardedCards.add(*m_context, card);

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
		{
			const Card& destroyedCard = m_context->getCard(_additionalEffect);
			// Track the destroyed card for potential Mausoleum revival
			m_discardedCards.add(*m_context, destroyedCard);
			otherPlayer.removeCard(destroyedCard);
		}

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

	SpecialAction GameState::pickScienceToken(u32 _tokenIndex, bool obtainedFromGreatLibrary)
	{
		ScienceToken pickedToken = m_scienceTokens[obtainedFromGreatLibrary ? (_tokenIndex + 5) : _tokenIndex];
		if (!obtainedFromGreatLibrary) {
			std::swap(m_scienceTokens[_tokenIndex], m_scienceTokens[m_numScienceToken - 1]);
			m_numScienceToken--;
		}

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

		updatePlaybleCardPtrDebug();
	}

	u32 GameState::computeNumDicoveriesIfPicked(u32 _playableCardId) const
	{
		DEBUG_ASSERT(_playableCardId < m_graph.m_numPlayableCards);
		u8 pickedCard = m_graph.m_playableCards[_playableCardId];

		const CardNode& node = m_graph.m_graph[pickedCard];
		u32 discoveries = 0;

		// helper lambda to test a single parent (avoid double counting if parents are the same)
		auto testParent = [&](u8 parent) {
			const CardNode& pnode = m_graph.m_graph[parent];
			// only non-visible parents become discoveries
			if (pnode.m_visible != 0)
				return;

			u32 child0 = pnode.m_child0 == pickedCard ? CardNode::InvalidNode : pnode.m_child0;
			u32 child1 = pnode.m_child1 == pickedCard ? CardNode::InvalidNode : pnode.m_child1;

			if (child0 == CardNode::InvalidNode && child1 == CardNode::InvalidNode)
				++discoveries;
		};

		if (node.m_parent0 != CardNode::InvalidNode)
			testParent(node.m_parent0);
		if (node.m_parent1 != CardNode::InvalidNode) {
			DEBUG_ASSERT(node.m_parent1 != node.m_parent0);
			testParent(node.m_parent1);
		}

		return discoveries;
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
		u32 vp0 = m_playerCity[0].computeVictoryPoint(m_playerCity[1], true);
		u32 vp1 = m_playerCity[1].computeVictoryPoint(m_playerCity[0], true);
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

	u32 PlayerCity::computeVictoryPoint(const PlayerCity& _otherCity, bool includeGoldVP) const
	{
		u32 goldVP = 0;
		if (includeGoldVP) {
			goldVP = m_gold / 3;
			if (m_ownedGuildCards & (1 << u32(CardType::Count)))
				goldVP *= 2;
		}

		u32 guildVP = 0;
		for (const Card& card : m_context->getAllGuildCards())
		{
			if (card.getSecondaryType() < u32(CardType::Count) && m_ownedGuildCards & (1 << card.getSecondaryType()))
			{
				u32 myCityCount = m_numCardPerType[card.getSecondaryType()];
				u32 oppCityCount = _otherCity.m_numCardPerType[card.getSecondaryType()];

				// A bit hacky but implement the guild card 'GuildeDesArmateurs'
				if (card.getSecondaryType() == u32(CardType::Brown)) {
					myCityCount += m_numCardPerType[u32(CardType::Grey)];
					oppCityCount += _otherCity.m_numCardPerType[u32(CardType::Grey)];
				}

				guildVP += card.m_victoryPoints * std::max(myCityCount, oppCityCount);
			}
		}

		return m_victoryPoints + goldVP + guildVP;
	}

	template<typename T>
	u32 GameState::fillTensorData(T* _data, u32 _mainPlayer) const
	{
		u32 opponent = (_mainPlayer + 1) % 2;
		u32 i = 0;
		_data[i++] = (T)m_numTurnPlayed;
		_data[i++] = (T)(_mainPlayer == 0 ? m_military : -m_military);
		_data[i++] = (T)((militaryToken2[_mainPlayer] ? 1:0) + (militaryToken5[_mainPlayer] ? 1:0));
		_data[i++] = (T)((militaryToken2[opponent] ? 1:0) + (militaryToken5[opponent] ? 1:0));

		constexpr u32 numCardTypeInGraph = u32(CardType::Guild) + 1;
		for (u32 j = 0; j < (u32(ScienceToken::Count) + numCardTypeInGraph); ++j) {
			_data[i + j] = 0;
		}
		for (u32 j = 0; j < m_numScienceToken; ++j)
			_data[i + u32(m_scienceTokens[j])] = 1;

		i += u32(ScienceToken::Count);

		// A few info about card that could be revived by Mausoleum
		_data[i++] = (T)(m_discardedCards.bestBlueCardId != u8(-1) ? m_context->getCard(m_discardedCards.bestBlueCardId).getVictoryPoints() : 0);
		_data[i++] = (T)(m_discardedCards.bestMilitaryCardId != u8(-1) ? m_context->getCard(m_discardedCards.bestMilitaryCardId).getMilitary() : 0);
		_data[i++] = (T)m_discardedCards.numGuildCards;

		static_assert(u32(ScienceSymbol::Law) == u32(ScienceSymbol::Count) - 1); // Law cannot be discarded
		for (u32 j = 0; j < u32(ScienceSymbol::Count)-1; ++j) {
			_data[i++] = (T)m_discardedCards.scienceCardIds[j];
		}
		
		for (const CardNode& node : m_graph.m_graph) {
			if (node.m_visible) {
				CardType type = m_context->getCard(node.m_cardId).getType();
				if (u32(type) < numCardTypeInGraph) {
					_data[i + u32(type)] += 1;
				}
			}
		}

		i += numCardTypeInGraph;

		const PlayerCity& myCity = m_playerCity[_mainPlayer];
		const PlayerCity& opponentCity = m_playerCity[(_mainPlayer + 1) % 2];

		// Maybe we can considere adding this
		// for (u8 j = 0; j < u8(Wonders::Count); ++j)
		// {
		//     if (std::find(myCity.m_unbuildWonders.begin(), myCity.m_unbuildWonders.end(), (Wonders)j) != myCity.m_unbuildWonders.end())
		//         _data[i] = 1;
		//     else if (std::find(opponentCity.m_unbuildWonders.begin(), opponentCity.m_unbuildWonders.end(), (Wonders)j) != opponentCity.m_unbuildWonders.end())
		//         _data[i] = -1;
		//     else
		//         _data[i] = 0;
		// 
		//     i++;
		// }

		_data[i++] = (T)myCity.computeVictoryPoint(opponentCity, false);
		_data[i++] = (T)opponentCity.computeVictoryPoint(myCity, false);

		auto fillCity = [&](const PlayerCity& _city)
		{
			u32 chainingSymbolCounts[4] = {};
			for (u8 j = 0; j < u8(ChainingSymbol::Count); ++j) {
				if ((_city.m_chainingSymbols & (1 << j)) > 0) {
					if (j >= u32(ChainingSymbol::FirstYellow) && j <= u32(ChainingSymbol::LastYellow))
						chainingSymbolCounts[0]++;
					else if (j >= u32(ChainingSymbol::FirstBlue) && j <= u32(ChainingSymbol::LastBlue))
						chainingSymbolCounts[1]++;
					else if (j >= u32(ChainingSymbol::FirstRed) && j <= u32(ChainingSymbol::LastRed))
						chainingSymbolCounts[2]++;
					else if (j >= u32(ChainingSymbol::FirstGreen) && j <= u32(ChainingSymbol::LastGreen))
						chainingSymbolCounts[3]++;
				}
			}

			_data[i++] = (T)chainingSymbolCounts[0];
			_data[i++] = (T)chainingSymbolCounts[1];
			_data[i++] = (T)chainingSymbolCounts[2];
			_data[i++] = (T)chainingSymbolCounts[3];

			for (u8 j = 0; j < u8(ScienceToken::CountForNN); ++j)
				_data[i++] = (T)((_city.m_ownedScienceTokens & (1 << j)) > 0 ? 1 : 0);

			_data[i++] = _city.m_numScienceSymbols;
			_data[i++] = (T)_city.m_gold;
			_data[i++] = (T)_city.m_numCardPerType[u32(CardType::Yellow)];

			for (u8 j = 0; j < u8(ResourceType::Count); ++j) {
				_data[i++] = (T)_city.m_production[j];
				_data[i++] = (T)(_city.m_resourceDiscount[j] ? 1 : 0);
			}

			// Overview of number of cards per type already taken (important to predict future plays, odd of military wins...)
			for (CardType t : { CardType::Yellow, CardType::Blue, CardType::Military, CardType::Science, CardType::Guild }) {
				_data[i++] = (T)_city.m_numCardPerType[u32(t)];
			}

			_data[i++] = (T)_city.m_weakProduction.first;
			_data[i++] = (T)_city.m_weakProduction.second;

			T numPlayAgainWonders = 0;
			for (u8 j = 0; j < _city.m_unbuildWonderCount; ++j) {
				if (Helper::isReplayWonder(_city.m_unbuildWonders[j]) || _city.ownScienceToken(ScienceToken::Theology))
					numPlayAgainWonders += 1;
			}
			_data[i++] = numPlayAgainWonders;
		};

		fillCity(myCity);
		fillCity(opponentCity);

		DEBUG_ASSERT(i == TensorSize);

		return i;
	}

	template u32 GameState::fillTensorData<float>(float* _data, u32 _mainPlayer) const;
	template u32 GameState::fillTensorData<int16_t>(int16_t* _data, u32 _mainPlayer) const;

	template<typename T>
	void GameState::fillExtraTensorData(T* _data) const
	{
		memset(_data, 0, ExtraTensorSize * sizeof(T));

		switch (m_state) {
		case State::DraftWonder:
			break;
		case State::Play:
			*(_data++) = (T)0;
			for (u32 i = 0; i < m_graph.m_numPlayableCards; ++i)
				fillTensorDataForPlayableCard(_data + i * TensorSizePerPlayableCard, i, m_playerTurn);
			for (u32 i = m_graph.m_numPlayableCards; i < 6; ++i) {
				for (u32 j = 0; j < TensorSizePerPlayableCard; ++j)
					_data[i * TensorSizePerPlayableCard + j] = T(-1);
			}
				

			_data += 6 * TensorSizePerPlayableCard;
			{
				// Also fill unbuilt wonder for the current player to let NN compute move policy for wonder building
				u32 unbuildWCount = m_playerCity[m_playerTurn].m_unbuildWonderCount;
				for (u32 i = 0; i < unbuildWCount; ++i) {
					Wonders wonderType = m_playerCity[m_playerTurn].m_unbuildWonders[i];
					const Card& wonder = m_context->getWonder(wonderType);
					_data[i * TensorSizePerWonder + 0] = (T)wonder.m_victoryPoints;
					_data[i * TensorSizePerWonder + 1] = (T)wonder.m_military;
					_data[i * TensorSizePerWonder + 2] = (Helper::isReplayWonder(wonderType) || m_playerCity[m_playerTurn].ownScienceToken(ScienceToken::Theology)) ? (T)1 : (T)0;
					_data[i * TensorSizePerWonder + 3] = (T)(wonder.m_isWeakProduction ? wonder.m_production[u32(RT::Wood)] : 0);
					_data[i * TensorSizePerWonder + 4] = (T)(wonder.m_isWeakProduction ? wonder.m_production[u32(RT::Glass)] : 0);
					_data[i * TensorSizePerWonder + 5] = (T)(wonder.m_goldReward);
					_data[i * TensorSizePerWonder + 6] = (T)((wonderType == Wonders::Zeus || wonderType == Wonders::CircusMaximus) ? 1 : 0);
					_data[i * TensorSizePerWonder + 7] = (T)(wonderType == Wonders::GreatLibrary ? 1 : 0);
					_data[i * TensorSizePerWonder + 8] = (T)(wonderType == Wonders::Mausoleum ? 1 : 0);
					_data[i * TensorSizePerWonder + 9] = (T)(m_playerCity[m_playerTurn].computeCost(wonder, m_playerCity[(m_playerTurn + 1) % 2]));
				}
				for (u32 i = unbuildWCount; i < 4; ++i) {
					for (u32 j = 0; j < TensorSizePerWonder; ++j)
						_data[i * TensorSizePerWonder + j] = T(-1);
				}
			}
			_data += 4 * TensorSizePerWonder;

			break;
		case State::PickScienceToken:
		case State::GreatLibraryTokenThenReplay:
		case State::GreatLibraryToken:
			static_assert(u32(ScienceToken::Count) * 5 + 1 <= ExtraTensorSize);
			*(_data++) = (T)1; // indicate science token picking
			{
				u32 poolBegin = m_state == State::PickScienceToken ? 0 : 5;
				u32 poolEnd = m_state == State::PickScienceToken ? m_numScienceToken : poolBegin + 3;
				for (u32 i = poolBegin; i < poolEnd; ++i)
				{
					for (u32 j = 0; j < u32(ScienceToken::Count); ++j) {
						if (m_scienceTokens[i] == ScienceToken(j))
							_data[(i - poolBegin) * u32(ScienceToken::Count) + j] = (T)1;
						else
							_data[(i - poolBegin) * u32(ScienceToken::Count) + j] = (T)0;
					}
				}
			}
			break;
		default:
			break;
		}
	}

	template void GameState::fillExtraTensorData<float>(float* _data) const;
	template void GameState::fillExtraTensorData<int16_t>(int16_t* _data) const;

	template<typename T>
	void GameState::fillTensorDataForPlayableCard(T* _data, u32 playableCard, u32 mainPlayer) const
	{
		int i = 0;
		const PlayerCity& myCity = m_playerCity[mainPlayer];
		const PlayerCity& opponentCity = m_playerCity[(mainPlayer + 1) % 2];

		const Card& card = getPlayableCard(playableCard);

		// Both guild and yellow card have long term (implicit) impact, so we give them specific inputs
		_data[i++] = (T)((card.getType() == CardType::Yellow) ? 1 : 0);
		_data[i++] = (T)((card.getType() == CardType::Guild) ? 1 : 0);

		for (u32 j = 0; j < u32(RT::Count); ++j)
			_data[i + j] = card.m_production[j];
		i += u32(RT::Count);

		if (card.m_science < ScienceSymbol::Count) {
			_data[i++] = (T)(myCity.m_ownedScienceSymbol[u32(card.m_science)] > 0 ? -1:1);
			_data[i++] = (T)(opponentCity.m_ownedScienceSymbol[u32(card.m_science)] > 0 ? -1:1);
		} else {
			_data[i++] = (T)0;
			_data[i++] = (T)0;
		}

		u32 goldReward = 0;
		if (myCity.ownScienceToken(ScienceToken::TownPlanning) && card.m_chainIn != ChainingSymbol::None && myCity.m_chainingSymbols & (1u << u32(card.m_chainIn)))
			goldReward += 4;

		if (card.m_goldPerNumberOfCardColorTypeCard)
			goldReward += myCity.m_numCardPerType[card.m_secondaryType] * card.m_goldReward;
		else if (card.m_type == CardType::Guild && card.m_secondaryType < u32(CardType::Count))
			goldReward += std::max(myCity.m_numCardPerType[card.m_secondaryType], opponentCity.m_numCardPerType[card.m_secondaryType]) * card.m_goldReward;
		else
			goldReward += card.m_goldReward;

		u32 vp = 0;
		if (card.m_type != CardType::Guild) {
			vp += card.m_victoryPoints;
		} else {
			if (card.getSecondaryType() < u32(CardType::Count)) {
				for (const Card& guildCard : m_context->getAllGuildCards()) {
					if (card.getId() == guildCard.getId()) {
						u32 numCards = std::max(myCity.m_numCardPerType[guildCard.getSecondaryType()], opponentCity.m_numCardPerType[guildCard.getSecondaryType()]);
						vp += guildCard.m_victoryPoints * numCards;
						break;
					}
				}
			} else {
				// Guild card giving VP per 3 gold, its temporary, money may be spent later. But I think its usefull for the NN to have this info.
				vp += goldReward / 3;
			}
		}

		_data[i++] = (T)vp;
		_data[i++] = (T)goldReward;
		_data[i++] = (T)card.m_military;
		_data[i++] = (T)((card.m_chainOut != ChainingSymbol::None) ? 1 : 0);
		_data[i++] = (T)(card.m_isWeakProduction ? 1 : 0);
		_data[i++] = (T)(card.m_isResourceDiscount ? 1 : 0);
		_data[i++] = (T)myCity.computeCost(card, opponentCity);
		_data[i++] = (T)opponentCity.computeCost(card, myCity);
		_data[i++] = (T)computeNumDicoveriesIfPicked(playableCard);

		DEBUG_ASSERT(i == TensorSizePerPlayableCard);
	}

	template void GameState::fillTensorDataForPlayableCard<float>(float* _data, u32 playableCard, u32 mainPlayer) const;
	template void GameState::fillTensorDataForPlayableCard<int16_t>(int16_t* _data, u32 playableCard, u32 mainPlayer) const;

	//----------------------------------------------------------------------------
	// DiscardedCards implementation
	//----------------------------------------------------------------------------
	DiscardedCards::DiscardedCards()
	{
		for (u8& id : bestProductionCardId)
			id = u8(-1);
		for (u8& id : scienceCardIds)
			id = u8(-1);
		for (u8& id : guildCardIds)
			id = u8(-1);
		for (u8& id : yellowResourceDiscountCardIds)
			id = u8(-1);
		for (u8& id : yellowGoldPerCardTypeCardIds)
			id = u8(-1);
	}

	void DiscardedCards::add(const GameContext& context, const Card& card)
	{
		u8 cardId = card.getId();
		CardType type = card.getType();

		switch (type)
		{
		case CardType::Brown:
		case CardType::Grey:
		{
			// Track best production card per resource type
			for (u32 r = 0; r < u32(ResourceType::Count); ++r)
			{
				if (card.m_production[r] > 0) {
					u8 existingId = bestProductionCardId[r];
					if (existingId == u8(-1))
						bestProductionCardId[r] = cardId;
					else {
						const Card& existing = context.getCard(existingId);
						if (card.m_production[r] > existing.m_production[r])
							bestProductionCardId[r] = cardId;
					}
				}
			}
			break;
		}

		case CardType::Blue:
		{
			if (bestBlueCardId == u8(-1))
				bestBlueCardId = cardId;
			else {
				const Card& existing = context.getCard(bestBlueCardId);
				if (card.m_victoryPoints > existing.m_victoryPoints)
					bestBlueCardId = cardId;
			}
			break;
		}

		case CardType::Military:
		{
			if (bestMilitaryCardId == u8(-1))
				bestMilitaryCardId = cardId;
			else {
				const Card& existing = context.getCard(bestMilitaryCardId);
				if (card.m_military > existing.m_military)
					bestMilitaryCardId = cardId;
			}
			break;
		}

		case CardType::Science:
		{
			// Track one card per science symbol
			u32 symbolIndex = u32(card.m_science);
			if (symbolIndex < scienceCardIds.size())
				scienceCardIds[symbolIndex] = cardId;
			break;
		}

		case CardType::Guild:
		{
			if (numGuildCards < guildCardIds.size())
				guildCardIds[numGuildCards++] = cardId;
			break;
		}

		case CardType::Yellow:
		{
			// Track best gold reward card (direct gold, not per-card-type)
			if (card.m_goldReward > 0 && !card.m_goldPerNumberOfCardColorTypeCard) {
				if (bestYellowGoldRewardCardId == u8(-1))
					bestYellowGoldRewardCardId = cardId;
				else {
					const Card& existing = context.getCard(bestYellowGoldRewardCardId);
					if (card.m_goldReward > existing.m_goldReward)
						bestYellowGoldRewardCardId = cardId;
				}
			}

			// Track best weak production cards (Caravanserail, Forum)
			if (card.m_isWeakProduction) {
				bool isRare = (card.m_production[u32(ResourceType::Glass)] > 0 || 
							  card.m_production[u32(ResourceType::Papyrus)] > 0);
				
				u8& target = isRare ? bestYellowWeakRareCardId : bestYellowWeakNormalCardId;
				if (target == u8(-1))
					target = cardId;
			}

			// Track resource discount cards (all unique cards, no duplicates)
			// DepotBois, DepotArgile, DepotPierre, Douane
			if (card.m_isResourceDiscount) {
				// Check if this card is already tracked (avoid duplicates)
				bool alreadyTracked = false;
				for (u32 i = 0; i < numYellowResourceDiscountCards; ++i) {
					if (yellowResourceDiscountCardIds[i] == cardId) {
						alreadyTracked = true;
						break;
					}
				}
				
				// Add if not already tracked and we have space
				if (!alreadyTracked && numYellowResourceDiscountCards < yellowResourceDiscountCardIds.size()) {
					yellowResourceDiscountCardIds[numYellowResourceDiscountCards++] = cardId;
				}
			}

			// Track yellow cards with gold-per-card-type (Armurerie, Phare, Port, ChambreDeCommerce, Arene)
			if (card.m_goldPerNumberOfCardColorTypeCard) {
				// Check if this card is already tracked (avoid duplicates)
				bool alreadyTracked = false;
				for (u32 i = 0; i < numYellowGoldPerCardTypeCards; ++i) {
					if (yellowGoldPerCardTypeCardIds[i] == cardId) {
						alreadyTracked = true;
						break;
					}
				}
				
				// Add if not already tracked and we have space
				if (!alreadyTracked && numYellowGoldPerCardTypeCards < yellowGoldPerCardTypeCardIds.size()) {
					yellowGoldPerCardTypeCardIds[numYellowGoldPerCardTypeCards++] = cardId;
				}
			}
			break;
		}

		default:
			break;
		}
	}

	void DiscardedCards::getRevivableCards(std::vector<u8>& outCardIds) const
	{
		outCardIds.clear();

		// Add best production cards
		for (u8 id : bestProductionCardId)
		{
			if (id != u8(-1))
				outCardIds.push_back(id);
		}

		// Add best blue card
		if (bestBlueCardId != u8(-1))
			outCardIds.push_back(bestBlueCardId);

		// Add best military card
		if (bestMilitaryCardId != u8(-1))
			outCardIds.push_back(bestMilitaryCardId);

		// Add science cards
		for (u8 id : scienceCardIds)
		{
			if (id != u8(-1))
				outCardIds.push_back(id);
		}

		// Add all guild cards
		for (u32 i = 0; i < numGuildCards; ++i)
		{
			if (guildCardIds[i] != u8(-1))
				outCardIds.push_back(guildCardIds[i]);
		}

		// Add yellow cards (all categories)
		if (bestYellowGoldRewardCardId != u8(-1))
			outCardIds.push_back(bestYellowGoldRewardCardId);
		if (bestYellowWeakNormalCardId != u8(-1))
			outCardIds.push_back(bestYellowWeakNormalCardId);
		if (bestYellowWeakRareCardId != u8(-1))
			outCardIds.push_back(bestYellowWeakRareCardId);
		
		// Add all unique resource discount cards (no duplicates)
		for (u32 i = 0; i < numYellowResourceDiscountCards; ++i)
		{
			if (yellowResourceDiscountCardIds[i] != u8(-1))
				outCardIds.push_back(yellowResourceDiscountCardIds[i]);
		}
		
		// Add all unique gold-per-card-type cards (no duplicates)
		for (u32 i = 0; i < numYellowGoldPerCardTypeCards; ++i)
		{
			if (yellowGoldPerCardTypeCardIds[i] != u8(-1))
				outCardIds.push_back(yellowGoldPerCardTypeCardIds[i]);
		}
	}

	bool DiscardedCards::hasRevivableCards() const
	{
		// Check if any card has been discarded
		for (u8 id : bestProductionCardId)
			if (id != u8(-1)) return true;
		
		if (bestBlueCardId != u8(-1)) return true;
		if (bestMilitaryCardId != u8(-1)) return true;
		
		for (u8 id : scienceCardIds)
			if (id != u8(-1)) return true;
		
		if (numGuildCards > 0) return true;
		
		if (bestYellowGoldRewardCardId != u8(-1)) return true;
		if (bestYellowWeakNormalCardId != u8(-1)) return true;
		if (bestYellowWeakRareCardId != u8(-1)) return true;
		if (numYellowResourceDiscountCards > 0) return true;
		if (numYellowGoldPerCardTypeCards > 0) return true;
		
		return false;
	}
}
