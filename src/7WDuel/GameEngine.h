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
		std::array<bool, u32(CardType::Count)> m_resourceDiscount = {};
		std::array<u8, u32(ResourceType::Count)> m_bestProductionCardId;
		std::array<Wonders, 4> m_unbuildWonders = {};
		u8 m_unbuildWonderCount = 4;

		PlayerCity(const GameContext* _context) : m_context{ _context } 
		{
			for (u8& index : m_bestProductionCardId)
				index = u8(-1);
		}

		u32 computeCost(const Card& _card, const PlayerCity& _otherPlayer) const;
		SpecialAction addCard(const Card& _card, const PlayerCity& _otherCity);
		void removeCard(const Card& _card);

		bool ownScienceToken(ScienceToken _token) const { return ((1u << u32(_token)) & m_ownedScienceTokens) > 0; }
		u32 computeVictoryPoint(const PlayerCity& _otherCity) const;

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
		GameState();
		GameState(const GameContext& _context);
		~GameState() = default;

		GameState(const GameState&) = default;
		GameState& operator=(const GameState&) = default;
		GameState(GameState&&) = default;
		GameState& operator=(GameState&&) = default;

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
		const Card& getPlayableScienceToken(u32 _index) const;
		const Card& getCurrentPlayerWonder(u32 _index) const;

		const PlayerCity& getPlayerCity(u32 _player) const { return m_playerCity[_player]; };

		std::array<ScienceToken, 5> getUnusedScienceToken() const;

		SpecialAction pick(u32 _playableCardIndex);
		void burn(u32 _playableCardIndex);
		SpecialAction buildWonder(u32 _withPlayableCardIndex, u32 _wondersIndex, u8 _additionalEffect = u8(-1));
		SpecialAction pickScienceToken(u32 _tokenIndex);
		u32 findWinner();

		std::ostream& printGameState(std::ostream& out) const;
		std::ostream& printPlayablCards(std::ostream& out) const;
		std::ostream& printAvailableTokens(std::ostream& out) const;

		static const u32 TensorSize = 165;
		template<typename T>
		u32 fillTensorData(T* _data, u32 _mainPlayer) const;

		static const u32 ExtraTensorSize = GameContext::MaxCardsPerAge * 2;
		template<typename T>
		void fillExtraTensorData(T* _data) const;

	private:
		
		struct CardNode
		{
			static constexpr u32 InvalidNode = 0x1F;

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

		std::array<CardNode, 20> m_graph;
		std::array<u8, 6> m_playableCards; // index in m_graph
		u8 m_numPlayableCards;

		std::array<u8, 23> m_availableAgeCards;
		std::array<u8, 7> m_availableGuildCards;
		u8 m_numAvailableAgeCards = 0;
		u8 m_numAvailableGuildCards = 0;

		std::array<u8, GameContext::MaxCardsPerAge> m_playedAgeCards;
		u8 m_numPlayedAgeCards = 0;

		u8 m_numTurnPlayed = 0;
		u8 m_playerTurn = 0;
		u8 m_currentAge = u8(-1);
		int8_t m_military = 0;
		bool militaryToken2[2] = { false,false };
		bool militaryToken5[2] = { false,false };

	private:
		u32 genPyramidGraph(u32 _numRow, u32 _startNodeIndex);
		u32 genInversePyramidGraph(u32 _baseSize, u32 _numRow, u32 _startNodeIndex);

		PlayerCity& getCurrentPlayerCity() { return m_playerCity[m_playerTurn]; }
		const PlayerCity& getCurrentPlayerCity() const { return m_playerCity[m_playerTurn]; }
		const PlayerCity& getOtherPlayerCity() const { return m_playerCity[(m_playerTurn + 1) % 2]; }
		void unlinkNodeFromGraph(u32 _nodeIndex);

		void initScienceTokens();
		void initAge1Graph();
		void initAge2Graph();
		void initAge3Graph();

		template<typename T>
		u8 pickCardIndex(T& _availableCards, u8& _numAvailableCard);

		void pickCardAdnInitNode(CardNode& _node);
	};

	template<typename T>
	u8 GameState::pickCardIndex(T& _availableCards, u8& _numAvailableCard)
	{
		u32 index = m_context->rand()() % _numAvailableCard;

		u8 cardIndex = _availableCards[index];
		std::swap(_availableCards[index], _availableCards[_numAvailableCard - 1]);
		_numAvailableCard--;
		return cardIndex;
	}

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
	}
}