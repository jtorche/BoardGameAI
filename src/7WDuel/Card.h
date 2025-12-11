#pragma once

#include "7WConstants.h"

namespace sevenWD
{
	//----------------------------------------------------------------------------
	class Card
	{
	public:
		Card() = default;
		Card(CardTag<CardType::Blue>, const char* _name, u8 _victoryPoints);
		Card(CardTag<CardType::Brown>, const char* _name, ResourceType _resource, u8 _num);
		Card(CardTag<CardType::Grey>, const char* _name, ResourceType _resource);
		Card(CardTag<CardType::Military>, const char* _name, u8 _numShields);
		Card(CardTag<CardType::Yellow>, const char* _name, u8 _victoryPoints);
		Card(CardTag<CardType::Science>, const char* _name, ScienceSymbol _science, u8 _victoryPoints);
		Card(CardTag<CardType::Guild>, const char* _name, CardType _cardColorForBonus, u8 _goldReward, u8 _victoryPointReward);
		Card(ScienceToken _scienceToken, const char* _name, u8 _goldReward = 0, u8 _victoryPointReward = 0);
		Card(Wonders _wonders, const char* _name, u8 _victoryPointReward, bool _extraTurn = false);

		u8 getId() const { return m_id; }
		u8 getAgeId() const { return m_ageId; }
		u8 getMilitary() const { return m_military; }
		u8 getGoldCost() const { return m_goldCost; }

		CardType getType() const { return m_type; }
		u8 getSecondaryType() const { return m_secondaryType; }

		void setId(u8 _id, u8 _ageId);
		Card& setResourceDiscount(ResourceSet _resources);
		Card& setWeakResourceProduction(ResourceSet _resources);
		Card& setMilitary(u8 _shield);
		Card& setGoldReward(u8 _reward);
		Card& setGoldRewardForCardColorCount(u8 _gold, CardType _typeRewarded);

		Card& setChainIn(ChainingSymbol _symbol);
		Card& setChainOut(ChainingSymbol _symbol);
		Card& setResourceCost(ResourceSet _cost);
		Card& setGoldCost(u8 _num);

		const char* getName() const { return m_name; }
		std::ostream& print(std::ostream& out) const;

		static constexpr u32 TensorSize = 44;

		template<typename T>
		void fillTensor(T* pData) const;

	private:
		const char* m_name = nullptr;
		u8 m_id = u8(-1);
		u8 m_ageId = u8(-1);
		CardType m_type = CardType::Count;

		ChainingSymbol m_chainIn = ChainingSymbol::None;
		ChainingSymbol m_chainOut = ChainingSymbol::None;
		std::array<u8, u32(RT::Count)> m_production = {};
		u8 m_goldReward = 0;
		bool m_isWeakProduction = false;
		bool m_isResourceDiscount = false;

		std::array<u8, u32(RT::Count)> m_cost = {};
		u8 m_goldCost = 0;
		u8 m_victoryPoints = 0;
		u8 m_military = 0;
		ScienceSymbol m_science = ScienceSymbol::Count;
		bool m_goldPerNumberOfCardColorTypeCard = false; // special bit indicating that the card reward gold m_goldReward per number of card of type m_secondaryType
		bool m_extraTurn = false;
		u8 m_secondaryType = 0; // for additional effects (like guild cards or wonders)

		friend struct PlayerCity;
		friend struct DiscardedCards;
	};
}
