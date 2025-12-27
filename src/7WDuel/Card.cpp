#include "7WDuel/Card.h"
#include "7WDuel/GameEngine.h"
#include <sstream>

namespace sevenWD
{
	//----------------------------------------------------------------------------
	Card::Card(CardTag<CardType::Blue>, const char* _name, u8 _victoryPoints)
		: m_type{ CardType::Blue }, m_name{ _name }, m_victoryPoints{ _victoryPoints }
	{
	}

	Card::Card(CardTag<CardType::Brown>, const char* _name, ResourceType _resource, u8 _num)
		: m_type{ CardType::Brown }, m_name{ _name }
	{
		m_production[u32(_resource)] = _num;
	}

	Card::Card(CardTag<CardType::Grey>, const char* _name, ResourceType _resource)
		: m_type{ CardType::Grey }, m_name{ _name }
	{
		m_production[u32(_resource)] = 1;
	}

	Card::Card(CardTag<CardType::Military>, const char* _name, u8 _numShields)
		: m_type{ CardType::Military }, m_name{ _name }, m_military{ _numShields }
	{
	}

	Card::Card(CardTag<CardType::Science>, const char* _name, ScienceSymbol _science, u8 _victoryPoints)
		: m_type{ CardType::Science }, m_name{ _name }, m_victoryPoints{ _victoryPoints }, m_science{ _science }
	{
	}

	Card::Card(CardTag<CardType::Guild>, const char* _name, CardType _cardColorForBonus, u8 _goldReward, u8 _victoryPointReward)
		: m_type{ CardType::Guild }, m_name{ _name }, m_victoryPoints{ _victoryPointReward }, m_goldReward{ _goldReward }
	{
		m_secondaryType = u8(_cardColorForBonus);
	}

	Card::Card(CardTag<CardType::Yellow>, const char* _name, u8 _victoryPoints)
		: m_type{ CardType::Yellow }, m_name{ _name }, m_victoryPoints{ _victoryPoints }
	{
	}

	Card::Card(ScienceToken _scienceToken, const char* _name, u8 _goldReward, u8 _victoryPointReward)
		: m_type{ CardType::ScienceToken }, m_name{ _name }, m_secondaryType{ u8(_scienceToken) }, m_goldReward{ _goldReward }, m_victoryPoints{ _victoryPointReward }
	{
		if (ScienceToken(m_secondaryType) == ScienceToken::Law)
			m_science = ScienceSymbol::Law;
	}

	Card::Card(Wonders _wonder, const char* _name, u8 _victoryPointReward, bool _extraTurn)
		: m_type{ CardType::Wonder }, m_name{ _name }, m_victoryPoints{ _victoryPointReward }, m_secondaryType{ u8(_wonder) }, m_extraTurn{ _extraTurn }
	{
	}

	Card& Card::setResourceDiscount(ResourceSet _resources)
	{
		m_isResourceDiscount = true;
		m_isWeakProduction = false;

		for (u8& r : m_production)
			r = 0;

		for (ResourceType type : _resources)
			m_production[u32(type)]++;

		return *this;
	}

	Card& Card::setWeakResourceProduction(ResourceSet _resources)
	{
		m_isResourceDiscount = false;
		m_isWeakProduction = true;

		for (u8& r : m_production)
			r = 0;

		for (ResourceType type : _resources)
			m_production[u32(type)]++;

		return *this;
	}

	Card& Card::setGoldReward(u8 _reward)
	{
		m_isResourceDiscount = false;
		m_isWeakProduction = false;

		for (u8& r : m_production)
			r = 0;

		m_goldReward = _reward;
		return *this;
	}

	Card& Card::setGoldRewardForCardColorCount(u8 _gold, CardType _typeRewarded)
	{
		m_goldPerNumberOfCardColorTypeCard = true;
		m_goldReward = _gold;
		m_secondaryType = u8(_typeRewarded);
		return *this;
	}

	Card& Card::setChainIn(ChainingSymbol _symbol)
	{
		m_chainIn = _symbol;
		return *this;
	}

	Card& Card::setChainOut(ChainingSymbol _symbol)
	{
		m_chainOut = _symbol;
		return *this;
	}

	Card& Card::setResourceCost(ResourceSet _cost)
	{
		for (u8& cost : m_cost)
			cost = 0;

		for (ResourceType type : _cost)
			m_cost[u32(type)]++;

		return *this;
	}

	Card& Card::setGoldCost(u8 _num)
	{
		m_goldCost = _num;
		return *this;
	}

	Card& Card::setMilitary(u8 _shield)
	{
		m_military = _shield;
		return *this;
	}

	//----------------------------------------------------------------------------
	void Card::setId(u8 _id, u8 _ageId)
	{
		m_id = _id;
		m_ageId = _ageId;
	}

	//----------------------------------------------------------------------------
	std::ostream& Card::print(std::ostream& out) const
	{
		out << "(" << Helper::toString(m_type) << ")" << m_name << "; Cost: ";

		bool firstCost = true;
		auto concatCost = [&](ResourceType _type)
		{
			if (m_cost[u32(_type)] > 0)
			{
				out << (firstCost ? "" : ", ") << u32(m_cost[u32(_type)]) << " " << Helper::toString(_type);
				firstCost = false;
			}
		};
		if (m_goldCost > 0)
		{
			out << (firstCost ? "" : ", ") << u32(m_goldCost) << " Gold";
			firstCost = false;
		}

		for (u32 i = 0; i < u32(ResourceType::Count); ++i)
			concatCost(ResourceType(i));

		return out;
	}
}
