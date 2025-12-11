#include "7WDuel/GameContext.h"
#include "7WDuel/GameEngine.h"

namespace sevenWD
{
	//----------------------------------------------------------------------------
	GameContext::GameContext(unsigned _seed) : m_rand(_seed)
	{
		m_allCards.clear();
		fillAge1();
		fillAge2();
		fillAge3();
		fillGuildCards();
		fillWonders();
		fillScienceTokens();
	}

	//----------------------------------------------------------------------------
	void GameContext::initCityWithRandomWonders(PlayerCity& _player1, PlayerCity& _player2) const
	{
		_player1.m_gold = 7;
		_player2.m_gold = 7;

		std::vector<Wonders> wonders((u32)Wonders::Count);
		for (u32 i = 0; i < wonders.size(); ++i)
			wonders[i] = Wonders(i);

		std::shuffle(wonders.begin(), wonders.end(), m_rand);

		_player1.m_unbuildWonderCount = 4;
		for (u32 i = 0; i < 4; ++i)
			_player1.m_unbuildWonders[i] = wonders[i];

		_player2.m_unbuildWonderCount = 4;
		for (u32 i = 0; i < 4; ++i)
			_player2.m_unbuildWonders[i] = wonders[4+i];
	}

	void GameContext::initCityWithFixedWonders(PlayerCity& _player1, PlayerCity& _player2) const
	{
		_player1.m_gold = 7;
		_player2.m_gold = 7;

		_player1.m_unbuildWonderCount = 4;
		_player1.m_unbuildWonders[0] = Wonders::Coloss;
		_player1.m_unbuildWonders[1] = Wonders::Atremis;
		_player1.m_unbuildWonders[2] = Wonders::HangingGarden;
		_player1.m_unbuildWonders[3] = Wonders::Pyramids;

		_player2.m_unbuildWonderCount = 4;
		_player2.m_unbuildWonders[0] = Wonders::CircusMaximus;
		_player2.m_unbuildWonders[1] = Wonders::Piraeus;
		_player2.m_unbuildWonders[2] = Wonders::Sphinx;
		_player2.m_unbuildWonders[3] = Wonders::GreatLighthouse;
			
	}

	void GameContext::fillAge1()
	{
		m_age1Cards.push_back(Card(CardTag<CardType::Blue>{}, "Autel", 3).setChainOut(ChainingSymbol::Moon));
		m_age1Cards.push_back(Card(CardTag<CardType::Blue>{}, "Bains", 3).setResourceCost({ RT::Stone }).setChainOut(ChainingSymbol::WaterDrop));
		m_age1Cards.push_back(Card(CardTag<CardType::Blue>{}, "Theater", 3).setChainOut(ChainingSymbol::Mask));

		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "Chantier",		 RT::Wood, 1));
		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "Exploitation",	 RT::Wood, 1).setGoldCost(1));
		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "BassinArgileux", RT::Clay, 1));
		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "Cavite",		 RT::Clay, 1).setGoldCost(1));
		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "Gisement",		 RT::Stone, 1));
		m_age1Cards.push_back(Card(CardTag<CardType::Brown>{}, "Mine",			 RT::Stone, 1).setGoldCost(1));

		m_age1Cards.push_back(Card(CardTag<CardType::Grey>{}, "Verrerie", RT::Glass).setGoldCost(1));
		m_age1Cards.push_back(Card(CardTag<CardType::Grey>{}, "Presse",	  RT::Papyrus).setGoldCost(1));
	     
		m_age1Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Taverne",		0).setGoldReward(4).setChainOut(ChainingSymbol::Jar));
		m_age1Cards.push_back(Card(CardTag<CardType::Yellow>{}, "DepotBois",    0).setGoldCost(3).setResourceDiscount({ RT::Wood }));
		m_age1Cards.push_back(Card(CardTag<CardType::Yellow>{}, "DepotArgile",	0).setGoldCost(3).setResourceDiscount({ RT::Clay }));
		m_age1Cards.push_back(Card(CardTag<CardType::Yellow>{}, "DepotPierre",	0).setGoldCost(3).setResourceDiscount({ RT::Stone }));
					
		m_age1Cards.push_back(Card(CardTag<CardType::Military>{}, "TourDeGarde", 1));
		m_age1Cards.push_back(Card(CardTag<CardType::Military>{}, "Caserne",	 1).setResourceCost({ RT::Clay }).setChainOut(ChainingSymbol::Sword));
		m_age1Cards.push_back(Card(CardTag<CardType::Military>{}, "Ecurie",		 1).setResourceCost({ RT::Wood }).setChainOut(ChainingSymbol::Horseshoe));
		m_age1Cards.push_back(Card(CardTag<CardType::Military>{}, "Palissade",	 1).setGoldCost(2).setChainOut(ChainingSymbol::Tower));

		m_age1Cards.push_back(Card(CardTag<CardType::Science>{}, "Apothicaire",	ScienceSymbol::Wheel,	 1).setResourceCost({ RT::Glass }));
		m_age1Cards.push_back(Card(CardTag<CardType::Science>{}, "Atelier",		ScienceSymbol::Triangle, 1).setResourceCost({ RT::Papyrus }));
		m_age1Cards.push_back(Card(CardTag<CardType::Science>{}, "Scriptorium", ScienceSymbol::Script,	 0).setGoldCost(2).setChainOut(ChainingSymbol::Book));
		m_age1Cards.push_back(Card(CardTag<CardType::Science>{}, "Officine",	ScienceSymbol::Bowl,	 0).setGoldCost(2).setChainOut(ChainingSymbol::Gear));

		DEBUG_ASSERT(m_age1Cards.size() <= MaxCardsPerAge);

		u8 localId = 0;
		for (Card& card : m_age1Cards)
		{
			card.setId(u8(m_allCards.size()), localId++);
			m_allCards.push_back(&card);
		}
	}

	void GameContext::fillAge2()
	{
		m_age2Cards.push_back(Card(CardTag<CardType::Blue>{}, "Tribunal", 5).setResourceCost({ RT::Wood, RT::Wood, RT::Glass }));
		m_age2Cards.push_back(Card(CardTag<CardType::Blue>{}, "Statue",	  4).setResourceCost({ RT::Clay, RT::Clay }).setChainIn(ChainingSymbol::Mask).setChainOut(ChainingSymbol::GreekPillar));
		m_age2Cards.push_back(Card(CardTag<CardType::Blue>{}, "Temple",   4).setResourceCost({ RT::Wood, RT::Papyrus }).setChainIn(ChainingSymbol::Moon).setChainOut(ChainingSymbol::Sun));
		m_age2Cards.push_back(Card(CardTag<CardType::Blue>{}, "Aqueduc",  5).setResourceCost({ RT::Stone, RT::Stone, RT::Stone }).setChainIn(ChainingSymbol::WaterDrop));
		m_age2Cards.push_back(Card(CardTag<CardType::Blue>{}, "Rostres",  4).setResourceCost({ RT::Stone, RT::Wood }).setChainOut(ChainingSymbol::Bank));

		m_age2Cards.push_back(Card(CardTag<CardType::Brown>{}, "Scierie", RT::Wood, 2).setGoldCost(2));
		m_age2Cards.push_back(Card(CardTag<CardType::Brown>{}, "Briquerie", RT::Clay, 2).setGoldCost(2));
		m_age2Cards.push_back(Card(CardTag<CardType::Brown>{}, "Carriere", RT::Stone, 2).setGoldCost(2));

		m_age2Cards.push_back(Card(CardTag<CardType::Grey>{}, "Soufflerie", RT::Glass));
		m_age2Cards.push_back(Card(CardTag<CardType::Grey>{}, "Sechoire", RT::Papyrus));

		m_age2Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Brasserie", 0).setGoldReward(6).setChainOut(ChainingSymbol::Barrel));
		m_age2Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Caravanserail", 0).setGoldCost(2).setResourceCost({ RT::Glass, RT::Papyrus }).setWeakResourceProduction({ RT::Wood, RT::Clay, RT::Stone }));
		m_age2Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Forum", 0).setGoldCost(3).setResourceCost({ RT::Clay }).setWeakResourceProduction({ RT::Glass, RT::Papyrus }));
		m_age2Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Douane", 0).setGoldCost(4).setResourceDiscount({ RT::Papyrus, RT::Glass }));

		m_age2Cards.push_back(Card(CardTag<CardType::Military>{}, "Haras", 1).setResourceCost({ RT::Clay, RT::Wood }).setChainIn(ChainingSymbol::Horseshoe));
		m_age2Cards.push_back(Card(CardTag<CardType::Military>{}, "Baraquements", 1).setGoldCost(3).setChainIn(ChainingSymbol::Sword));
		m_age2Cards.push_back(Card(CardTag<CardType::Military>{}, "ChampsDeTir", 2).setResourceCost({ RT::Stone, RT::Wood, RT::Papyrus }).setChainOut(ChainingSymbol::Target));
		m_age2Cards.push_back(Card(CardTag<CardType::Military>{}, "PlaceArmes", 2).setResourceCost({ RT::Clay, RT::Clay, RT::Glass }).setChainOut(ChainingSymbol::Helmet));
		m_age2Cards.push_back(Card(CardTag<CardType::Military>{}, "Muraille", 2).setResourceCost({ RT::Stone, RT::Stone }));

		m_age2Cards.push_back(Card(CardTag<CardType::Science>{}, "Ecole", ScienceSymbol::Wheel, 1).setResourceCost({ RT::Wood, RT::Papyrus, RT::Papyrus }).setChainOut(ChainingSymbol::Harp));
		m_age2Cards.push_back(Card(CardTag<CardType::Science>{}, "Laboratoire", ScienceSymbol::Triangle, 1).setResourceCost({ RT::Wood, RT::Glass, RT::Glass }).setChainOut(ChainingSymbol::Lamp));
		m_age2Cards.push_back(Card(CardTag<CardType::Science>{}, "Bibliotheque", ScienceSymbol::Script, 2).setResourceCost({ RT::Stone, RT::Wood, RT::Glass }).setChainIn(ChainingSymbol::Book));
		m_age2Cards.push_back(Card(CardTag<CardType::Science>{}, "Dispensaire", ScienceSymbol::Bowl, 2).setResourceCost({ RT::Clay, RT::Clay, RT::Stone }).setChainIn(ChainingSymbol::Gear));

		DEBUG_ASSERT(m_age2Cards.size() <= MaxCardsPerAge);

		u8 localId = 0;
		for (Card& card : m_age2Cards)
		{
			card.setId(u8(m_allCards.size()), localId++);
			m_allCards.push_back(&card);
		}
	}

	void GameContext::fillAge3()
	{
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "Senat", 5).setResourceCost({ RT::Clay, RT::Clay, RT::Stone, RT::Papyrus }).setChainIn(ChainingSymbol::Bank));
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "Obelisque", 5).setResourceCost({ RT::Stone, RT::Stone, RT::Glass }));
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "Jardins", 6).setResourceCost({ RT::Clay, RT::Clay, RT::Wood, RT::Wood }).setChainIn(ChainingSymbol::GreekPillar));
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "Pantheon", 6).setResourceCost({ RT::Clay, RT::Wood, RT::Papyrus, RT::Papyrus }).setChainIn(ChainingSymbol::Sun));
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "Palace", 7).setResourceCost({ RT::Clay, RT::Stone, RT::Wood, RT::Glass, RT::Glass }));
		m_age3Cards.push_back(Card(CardTag<CardType::Blue>{}, "HotelDeVille", 7).setResourceCost({ RT::Stone, RT::Stone, RT::Stone, RT::Wood, RT::Wood }));

		m_age3Cards.push_back(Card(CardTag<CardType::Military>{}, "Fortifications", 2).setResourceCost({ RT::Stone, RT::Stone, RT::Clay, RT::Papyrus }).setChainIn(ChainingSymbol::Tower));
		m_age3Cards.push_back(Card(CardTag<CardType::Military>{}, "Cirque", 2).setResourceCost({ RT::Clay, RT::Clay, RT::Stone, RT::Stone }).setChainIn(ChainingSymbol::Helmet));
		m_age3Cards.push_back(Card(CardTag<CardType::Military>{}, "AtelierDeSiege", 2).setResourceCost({ RT::Wood, RT::Wood, RT::Wood, RT::Glass }).setChainIn(ChainingSymbol::Target));
		m_age3Cards.push_back(Card(CardTag<CardType::Military>{}, "Arsenal", 3).setResourceCost({ RT::Clay, RT::Clay, RT::Clay, RT::Wood, RT::Wood }));
		m_age3Cards.push_back(Card(CardTag<CardType::Military>{}, "Pretoire", 3).setGoldCost(8));

		m_age3Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Armurerie", 3).setResourceCost({ RT::Stone, RT::Stone, RT::Glass }).setGoldRewardForCardColorCount(1, CardType::Military));
		m_age3Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Phare", 3).setResourceCost({ RT::Clay, RT::Clay, RT::Glass }).setGoldRewardForCardColorCount(1, CardType::Yellow).setChainIn(ChainingSymbol::Jar));
		m_age3Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Port", 3).setResourceCost({ RT::Wood, RT::Glass, RT::Papyrus }).setGoldRewardForCardColorCount(2, CardType::Brown));
		m_age3Cards.push_back(Card(CardTag<CardType::Yellow>{}, "ChambreDeCommerce", 3).setResourceCost({ RT::Papyrus, RT::Papyrus }).setGoldRewardForCardColorCount(3, CardType::Grey));
		m_age3Cards.push_back(Card(CardTag<CardType::Yellow>{}, "Arene", 3).setResourceCost({ RT::Clay, RT::Stone, RT::Wood }).setGoldRewardForCardColorCount(2, CardType::Wonder).setChainIn(ChainingSymbol::Barrel));

		m_age3Cards.push_back(Card(CardTag<CardType::Science>{}, "Observatoire", ScienceSymbol::Globe, 2).setResourceCost({ RT::Stone, RT::Papyrus, RT::Papyrus }).setChainIn(ChainingSymbol::Lamp));
		m_age3Cards.push_back(Card(CardTag<CardType::Science>{}, "University", ScienceSymbol::Globe, 2).setResourceCost({ RT::Clay, RT::Glass, RT::Papyrus }).setChainIn(ChainingSymbol::Harp));
		m_age3Cards.push_back(Card(CardTag<CardType::Science>{}, "Etude", ScienceSymbol::SolarClock, 3).setResourceCost({ RT::Wood, RT::Wood, RT::Glass, RT::Papyrus }));
		m_age3Cards.push_back(Card(CardTag<CardType::Science>{}, "Academie", ScienceSymbol::SolarClock, 3).setResourceCost({ RT::Stone, RT::Wood, RT::Glass, RT::Glass }));

		DEBUG_ASSERT((m_age3Cards.size() + m_guildCards.size()) <= MaxCardsPerAge);

		u8 localId = (u8)m_guildCards.size();
		for (Card& card : m_age3Cards)
		{
			card.setId(u8(m_allCards.size()), localId++);
			m_allCards.push_back(&card);
		}
	}

	void GameContext::fillGuildCards()
	{
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesArmateurs", CardType::Brown, 1, 1).setResourceCost({ RT::Clay, RT::Stone, RT::Glass, RT::Papyrus }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesCommercant", CardType::Yellow, 1, 1).setResourceCost({ RT::Clay, RT::Wood, RT::Glass, RT::Papyrus }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesTacticiens", CardType::Military, 1, 1).setResourceCost({ RT::Stone, RT::Stone, RT::Clay, RT::Papyrus }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesMagistrats", CardType::Blue, 1, 1).setResourceCost({ RT::Wood, RT::Wood, RT::Clay, RT::Papyrus }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesSciences", CardType::Science, 1, 1).setResourceCost({ RT::Clay, RT::Clay, RT::Wood, RT::Wood }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesBatisseurs", CardType::Wonder, 0, 2).setResourceCost({ RT::Stone, RT::Stone, RT::Clay, RT::Wood, RT::Glass }));
		m_guildCards.push_back(Card(CardTag<CardType::Guild>{}, "GuildeDesUsuriers", CardType::Count, 0, 0).setResourceCost({ RT::Stone, RT::Stone, RT::Wood, RT::Wood }));

		DEBUG_ASSERT((m_age3Cards.size() + m_guildCards.size()) <= MaxCardsPerAge);

		u8 localId = (u8)m_age3Cards.size();
		for (Card& card : m_guildCards)
		{
			card.setId(u8(m_allCards.size()), localId++);
			m_allCards.push_back(&card);
		}
	}

	void GameContext::fillWonders()
	{
		m_wonders.resize(u32(Wonders::Count));

		m_wonders[u32(Wonders::Coloss)] = Card(Wonders::Coloss, "LeColosse", 3).setMilitary(2).setResourceCost({RT::Clay, RT::Clay, RT::Clay, RT::Glass});
		m_wonders[u32(Wonders::Atremis)] = Card(Wonders::Atremis, "TempleArtemis", 0, true).setGoldReward(12).setResourceCost({ RT::Wood, RT::Stone, RT::Glass, RT::Papyrus });
		m_wonders[u32(Wonders::Pyramids)] = Card(Wonders::Pyramids, "LesPyramides", 9).setResourceCost({ RT::Papyrus, RT::Stone, RT::Stone, RT::Stone });
		m_wonders[u32(Wonders::Zeus)] = Card(Wonders::Zeus, "StatueDeZeus", 3).setMilitary(1).setResourceCost({ RT::Papyrus, RT::Papyrus, RT::Clay, RT::Wood, RT::Stone });
		m_wonders[u32(Wonders::GreatLighthouse)] = Card(Wonders::GreatLighthouse, "LeGrandPhare", 4).setWeakResourceProduction({ RT::Clay, RT::Stone, RT::Wood }).setResourceCost({ RT::Papyrus, RT::Papyrus, RT::Stone, RT::Wood });
		m_wonders[u32(Wonders::CircusMaximus)] = Card(Wonders::CircusMaximus, "CircusMaximus", 3).setMilitary(1).setResourceCost({ RT::Stone, RT::Stone, RT::Wood, RT::Glass });

		m_wonders[u32(Wonders::GreatLibrary)] = Card(Wonders::GreatLibrary, "GreatLibrary", 4).setResourceCost({ RT::Wood, RT::Wood, RT::Wood, RT::Glass, RT::Papyrus });
		m_wonders[u32(Wonders::Sphinx)] = Card(Wonders::Sphinx, "Sphinx", 6, true).setResourceCost({ RT::Stone, RT::Clay, RT::Glass, RT::Glass });
		m_wonders[u32(Wonders::ViaAppia)] = Card(Wonders::ViaAppia, "LaViaAppia", 3, true).setGoldReward(3).setResourceCost({ RT::Clay, RT::Clay, RT::Stone, RT::Stone, RT::Papyrus });
		m_wonders[u32(Wonders::Piraeus)] = Card(Wonders::Piraeus, "LaPiree", 2, true).setWeakResourceProduction({ RT::Papyrus, RT::Glass }).setResourceCost({ RT::Clay, RT::Stone, RT::Wood, RT::Wood });
		m_wonders[u32(Wonders::HangingGarden)] = Card(Wonders::HangingGarden, "JardinSuspendus", 3, true).setGoldReward(6).setResourceCost({ RT::Papyrus, RT::Glass, RT::Wood, RT::Wood });
		m_wonders[u32(Wonders::Mausoleum)] = Card(Wonders::Mausoleum, "Mausoleum", 2).setResourceCost({ RT::Papyrus, RT::Glass, RT::Glass, RT::Clay, RT::Clay });

		for (Card& card : m_wonders)
		{
			card.setId(u8(m_allCards.size()), 0xFF);
			m_allCards.push_back(&card);
		}
	}

	void GameContext::fillScienceTokens()
	{
		m_scienceTokens.resize(u32(ScienceToken::Count));

		m_scienceTokens[u32(ScienceToken::Agriculture)] = Card(ScienceToken::Agriculture, "Agriculture", 6, 4);
		m_scienceTokens[u32(ScienceToken::Architecture)] = Card(ScienceToken::Architecture, "Architecture");
		m_scienceTokens[u32(ScienceToken::Economy)] = Card(ScienceToken::Economy, "Economy");
		m_scienceTokens[u32(ScienceToken::Law)] = Card(ScienceToken::Law, "Law");
		m_scienceTokens[u32(ScienceToken::Masonry)] = Card(ScienceToken::Masonry, "Masonry");
		m_scienceTokens[u32(ScienceToken::Mathematics)] = Card(ScienceToken::Mathematics, "Mathematics");
		m_scienceTokens[u32(ScienceToken::Philosophy)] = Card(ScienceToken::Philosophy, "Philosophy", 0, 7);
		m_scienceTokens[u32(ScienceToken::Strategy)] = Card(ScienceToken::Strategy, "Strategy");
		m_scienceTokens[u32(ScienceToken::Theology)] = Card(ScienceToken::Theology, "Theology");
		m_scienceTokens[u32(ScienceToken::TownPlanning)] = Card(ScienceToken::TownPlanning, "TownPlanning", 6);

		for (Card& card : m_scienceTokens)
		{
			card.setId(u8(m_allCards.size()), 0xFF);
			m_allCards.push_back(&card);
		}
	}
}
