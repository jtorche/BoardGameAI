#pragma once

#include <array>
#include <iostream>
#include <vector>
#include <algorithm>

#include "GameContext.h"

namespace sevenWD
{
	//----------------------------------------------------------------------------
	struct PlayerCity
	{
		const GameContext* m_context;
		u32 m_chainingSymbols = 0; // bitfield
		u16 m_ownedGuildCards = 0; // bitfield
		u16 m_ownedScienceTokens = 0; // bitfield
		u8 m_numScienceSymbols = 0;
		u8 m_gold = 0;
		u8 m_victoryPoints = 0;
		std::array<u8, u32(ScienceSymbol::Count)> m_ownedScienceSymbol = {};
		std::array<u8, u32(CardType::Count)> m_numCardPerType = {};
		std::array<u8, u32(ResourceType::Count)> m_production = {};
		std::pair<u8, u8> m_weakProduction = {};
		std::array<bool, u32(CardType::Count)> m_resourceDiscount = {}; // Bug but need to update serialization as well if changing to ResourceType::Count
		std::array<u8, u32(ResourceType::Count)> m_bestProductionCardId;
		std::array<Wonders, 4> m_unbuildWonders = {};
		u8 m_unbuildWonderCount = 0;

		PlayerCity(const GameContext* _context) : m_context{ _context } 
		{
			for (u8& index : m_bestProductionCardId)
				index = u8(-1);
		}

		u32 computeCost(const Card& _card, const PlayerCity& _otherPlayer) const;
		SpecialAction addCard(const Card& _card, const PlayerCity& _otherCity);
		void removeCard(const Card& _card);

		bool ownScienceToken(ScienceToken _token) const { return ((1u << u32(_token)) & m_ownedScienceTokens) > 0; }
		u32 computeVictoryPoint(const PlayerCity& _otherCity, bool includeGoldVP) const;

		void print();
	};

	struct DiscardedCards
	{
		u8 militaryCard = u8(-1);
		u8 bestVictoryPoint = u8(-1);
		u8 scienceCards[u32(ScienceSymbol::Count)];
		u8 guildCards[3];
		u8 numGuildCards = 0;
	
		DiscardedCards();
		void add(const GameContext&, const Card&);
	};

	//----------------------------------------------------------------------------
	class GameState
	{
		friend struct GameController;

	public:
		enum class State : u8
		{
			DraftWonder,
			Play,
			PickScienceToken,
			GreatLibraryToken,
			GreatLibraryTokenThenReplay,
			WinPlayer0,
			WinPlayer1
		};

		GameState();
		GameState(const GameContext& _context);
		~GameState() = default;

		GameState(const GameState&) = default;
		GameState& operator=(const GameState&) = default;
		GameState(GameState&&) = default;
		GameState& operator=(GameState&&) = default;

		void makeDeterministic();

		enum class NextAge
		{
			None,
			Next,
			EndGame
		};
		u32 getCurrentAge() const { return m_currentAge; }
		NextAge nextAge();
		u32 getCurrentPlayerTurn() const { return m_playerTurn; }
		u8 getNumTurnPlayed() const { return m_numTurnPlayed; }
		void nextPlayer() { m_numTurnPlayed++; m_playerTurn = (m_playerTurn + 1) % 2; }

		const Card& getPlayableCard(u32 _index) const;
		const Card& getPlayableScienceToken(u32 _index, bool isGreatLibraryDraft) const;
		const Card& getCurrentPlayerWonder(u32 _index) const;

		const PlayerCity& getPlayerCity(u32 _player) const { return m_playerCity[_player]; };

		std::array<ScienceToken, 3> getGreatLibraryDraft() const;

		bool isDraftingWonders() const { return m_currentDraftRound < 2; }
		u8 getCurrentWonderDraftRound() const { return m_currentDraftRound; }
		u8 getNumDraftableWonders() const;
		Wonders getDraftableWonder(u32 _index) const;
		void draftWonder(u32 _draftIndex);

		unsigned int getNumPlayableCards() const { return m_graph.m_numPlayableCards; }
		SpecialAction pick(u32 _playableCardIndex);
		void burn(u32 _playableCardIndex);
		SpecialAction buildWonder(u32 _withPlayableCardIndex, u32 _wondersIndex, u8 _additionalEffect = u8(-1));
		SpecialAction pickScienceToken(u32 _tokenIndex, bool obtainedFromGreatLibrary);
		u32 findWinner();

		std::ostream& printGameState(std::ostream& out) const;
		std::ostream& printPlayablCards(std::ostream& out) const;
		std::ostream& printAvailableTokens(std::ostream& out) const;

		static const u32 TensorSize = 83;
		template<typename T>
		u32 fillTensorData(T* _data, u32 _mainPlayer) const;

		static const u32 TensorSizePerPlayableCard = 18;
		static const u32 TensorSizePerWonder = 9;
		template<typename T>
		void fillTensorDataForPlayableCard(T* _data, u32 playableCard, u32 mainPlayer) const;

		static const u32 ExtraTensorSize = 1 + TensorSizePerPlayableCard * 6 + TensorSizePerWonder * 4;
		template<typename T>
		void fillExtraTensorData(T* _data) const;

		int getMilitary() const { return m_military; }

		// Added getters for persistent military-token flags so const accessors are available
		bool getMilitaryToken2(u32 player) const { return militaryToken2[player]; }
		bool getMilitaryToken5(u32 player) const { return militaryToken5[player]; }

	public:
		
		struct CardNode
		{
			static constexpr u32 InvalidNode = 0x1F;
			static constexpr u32 InvalidCardId = 0x3FF;

			u32 m_parent0 : 5;
			u32 m_parent1 : 5;
			u32 m_child0 : 5;
			u32 m_child1 : 5;
			u32 m_cardId : 10;
			u32 m_visible : 1;
			u32 m_isGuildCard : 1;
		};

		const GameContext* m_context;
		std::array<PlayerCity, 2> m_playerCity;
		std::array<ScienceToken, u8(ScienceToken::Count)> m_scienceTokens;
		u8 m_numScienceToken = 0;
		bool m_isDeterministic = false;
		State m_state = State::DraftWonder;

		// Each graph a pre-determined if gameState is deterministic
		using GraphArray = std::array<CardNode, 20>;
		struct GraphSetup
		{
			GraphArray m_graph;
			std::array<u8, 6> m_playableCards; // index in m_graph
			std::array<u8, 23> m_availableAgeCards;
			std::array<u8, 7> m_availableGuildCards;
			u8 m_age;
			u8 m_numPlayableCards;
			u8 m_numAvailableAgeCards = 0;
			u8 m_numAvailableGuildCards = 0;
		};
		GraphSetup m_graphsPerAge[3]; // one per age
		GraphSetup m_graph; // active graph

#ifdef _DEBUG
		const Card* m_playableCardsPtr[6];
#endif
		void updatePlaybleCardPtrDebug();

		std::array<u8, GameContext::MaxCardsPerAge> m_playedAgeCards;
		u8 m_numPlayedAgeCards = 0;

		u8 m_numTurnPlayed = 0;
		u8 m_playerTurn = 0;
		u8 m_currentAge = u8(-1);
		int8_t m_military = 0;
		bool militaryToken2[2] = { false,false };
		bool militaryToken5[2] = { false,false };

		std::array<Wonders, u32(Wonders::Count) - 1> m_wonderDraftPool; // skip Mauselum for now
		u8 m_currentDraftRound = 0; // 0 = first round, 1 = second round, 2 = finished
		u8 m_picksInCurrentRound = 0;

	private:
		u32 genPyramidGraph(u32 _numRow, u32 _startNodeIndex, GraphArray& graph);
		u32 genInversePyramidGraph(u32 _baseSize, u32 _numRow, u32 _startNodeIndex, GraphArray& graph);

		PlayerCity& getCurrentPlayerCity() { return m_playerCity[m_playerTurn]; }
		const PlayerCity& getCurrentPlayerCity() const { return m_playerCity[m_playerTurn]; }
		const PlayerCity& getOtherPlayerCity() const { return m_playerCity[(m_playerTurn + 1) % 2]; }
		void unlinkNodeFromGraph(u32 _nodeIndex);
		void updateMilitary(u8 military, bool hasStrategyToken);

		void initScienceTokens();
		void initAge1Graph(bool makeDeterministic);
		void initAge2Graph(bool makeDeterministic);
		void initAge3Graph(bool makeDeterministic);

		void initAge1();
		void initAge2();
		void initAge3();

		void initWonderDraft();
		void finishWonderDraft();
		void pickCardAdnInitNode(CardNode& _node, GraphSetup& graph);
		u32 computeNumDicoveriesIfPicked(u32 playableCardId) const;

		template<typename T>
		u8 pickCardIndex(T& _availableCards, u8& _numAvailableCard)
		{
			u32 index = m_context->rand()() % _numAvailableCard;

			u8 cardIndex = _availableCards[index];
			if (m_currentAge == 2) {
				DEBUG_ASSERT(cardIndex<20);
			}
			std::swap(_availableCards[index], _availableCards[_numAvailableCard - 1]);
			_numAvailableCard--;
			return cardIndex;
		}
	};
 
 	namespace Helper
 	{
 		template<typename T>
 		T safeSub(T x, T y)
 		{
 			return x > y ? x - y : 0;
 		}

 		bool isReplayWonder(Wonders _wonder);

 		const char* toString(ResourceType _type);
 		const char* toString(CardType _type);
 		const char* toString(ScienceSymbol _type);

		std::vector<u8> serializeGameState(const GameState& _state);
		bool deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob, GameState& _outState);
 	}
 }