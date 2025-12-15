#pragma once
#include "Core/Common.h"
#include "Card.h"

#include <random>

namespace sevenWD
{
	struct PlayerCity;

	//----------------------------------------------------------------------------
	class GameContext
	{
	public:
		GameContext(unsigned int _seed = 42);

		void initCityWithRandomWonders(PlayerCity& _player1, PlayerCity& _player2) const;
		void initCityWithFixedWonders(PlayerCity& _player1, PlayerCity& _player2) const;

		std::default_random_engine& rand() const { return m_rand; }
		const Card& getCard(u8 _cardId) const { return *m_allCards[_cardId]; }
		const Card& getWonder(Wonders _wonder) const { return m_wonders[u32(_wonder)]; }
		const Card& getScienceToken(ScienceToken _token) const { return m_scienceTokens[u32(_token)]; }

		u8 getAge1CardCount() const { return u8(m_age1Cards.size()); }
		u8 getAge2CardCount() const { return u8(m_age2Cards.size()); }
		u8 getAge3CardCount() const { return u8(m_age3Cards.size()); }
		u8 getGuildCardCount() const { return u8(m_guildCards.size()); }

		const Card& getAge1Card(u32 _index) const { return m_age1Cards[_index]; }
		const Card& getAge2Card(u32 _index) const { return m_age2Cards[_index]; }
		const Card& getAge3Card(u32 _index) const { return m_age3Cards[_index]; }
		const Card& getGuildCard(u32 _index) const { return m_guildCards[_index]; }

		const std::vector<Card>& getAllGuildCards() const { return m_guildCards; }

		static constexpr u32 MaxCardsPerAge = 30;

	private:
		mutable std::default_random_engine m_rand;
		std::vector<Card> m_age1Cards;
		std::vector<Card> m_age2Cards;
		std::vector<Card> m_age3Cards;
		std::vector<Card> m_guildCards;
		std::vector<Card> m_wonders;
		std::vector<Card> m_scienceTokens;
		std::vector<const Card*> m_allCards;

	private:
		void fillAge1();
		void fillAge2();
		void fillAge3();
		void fillGuildCards();
		void fillWonders();
		void fillScienceTokens();
	};
}