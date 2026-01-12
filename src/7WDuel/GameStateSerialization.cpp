#include "7WDuel/GameEngine.h"

#include <vector>
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace sevenWD::Helper
{
	// Serializer for GameState -> binary blob (little-endian)
	// Format:
	// - 4 bytes magic: '7','W','G','S'
	// - 1 byte version
	// - remaining: fields in stable order (see implementation)
	std::vector<u8> serializeGameState(const GameState& _state)
	{
		std::vector<u8> out;
		out.reserve(512);

		auto writeLE = [&](auto value)
			{
				using T = std::decay_t<decltype(value)>;
				using UT = std::make_unsigned_t<T>;
				UT v = static_cast<UT>(value);
				for (size_t i = 0; i < sizeof(T); ++i)
					out.push_back(u8((v >> (8 * i)) & 0xFFu));
			};

		auto writeBytes = [&](const void* data, size_t size)
			{
				const u8* b = reinterpret_cast<const u8*>(data);
				out.insert(out.end(), b, b + size);
			};

		// header
		out.push_back('7');
		out.push_back('W');
		out.push_back('G');
		out.push_back('S');
		out.push_back(3); // version 3 (updated for DiscardedCards changes)

		// basic game state scalars
		writeLE(_state.m_state);
		writeLE(_state.m_numTurnPlayed);
		writeLE(_state.m_playerTurn);
		writeLE(_state.m_currentAge);
		writeLE(_state.m_military);
		writeLE(u8(_state.militaryToken2[0]));
		writeLE(u8(_state.militaryToken2[1]));
		writeLE(u8(_state.militaryToken5[0]));
		writeLE(u8(_state.militaryToken5[1]));

		// science tokens
		writeLE(_state.m_numScienceToken);
		for (u32 i = 0; i < u32(ScienceToken::Count); ++i)
		{
			// write all token enum values present in first m_numScienceToken positions,
			// then zeros for the rest: keep fixed-size representation
			u8 v = 0;
			if (i < _state.m_numScienceToken)
				v = u8(_state.m_scienceTokens[i]);
			writeLE(v);
		}

		// played age cards set
		writeLE(_state.m_numPlayedAgeCards);
		for (u32 i = 0; i < GameContext::MaxCardsPerAge; ++i)
			writeLE(_state.m_playedAgeCards[i]);

		// DiscardedCards tracking (NEW in version 3)
		{
			const auto& dc = _state.m_discardedCards;
			
			// Production cards (5 resource types)
			for (u32 i = 0; i < u32(ResourceType::Count); ++i)
				writeLE(dc.bestProductionCardId[i]);
			
			// Single cards
			writeLE(dc.bestBlueCardId);
			writeLE(dc.bestMilitaryCardId);
			
			// Science cards (7 symbols)
			for (u32 i = 0; i < u32(ScienceSymbol::Count); ++i)
				writeLE(dc.scienceCardIds[i]);
			
			// Guild cards (variable count, max 7)
			writeLE(dc.numGuildCards);
			for (u32 i = 0; i < dc.guildCardIds.size(); ++i)
				writeLE(dc.guildCardIds[i]);
			
			// Yellow cards
			writeLE(dc.bestYellowGoldRewardCardId);
			writeLE(dc.bestYellowWeakNormalCardId);
			writeLE(dc.bestYellowWeakRareCardId);
			
			// Yellow resource discount cards (variable count, max 4)
			writeLE(dc.numYellowResourceDiscountCards);
			for (u32 i = 0; i < dc.yellowResourceDiscountCardIds.size(); ++i)
				writeLE(dc.yellowResourceDiscountCardIds[i]);
			
			// Yellow gold-per-card-type cards (variable count, max 5)
			writeLE(dc.numYellowGoldPerCardTypeCards);
			for (u32 i = 0; i < dc.yellowGoldPerCardTypeCardIds.size(); ++i)
				writeLE(dc.yellowGoldPerCardTypeCardIds[i]);
		}

		// wonder draft pool and draft round info
		for (u32 i = 0; i < _state.m_wonderDraftPool.size(); ++i)
			writeLE(u8(_state.m_wonderDraftPool[i]));
		writeLE(_state.m_currentDraftRound);
		writeLE(_state.m_picksInCurrentRound);

		// player cities (2)
		for (u32 p = 0; p < 2; ++p)
		{
			const PlayerCity& city = _state.m_playerCity[p];

			writeLE(city.m_chainingSymbols);
			writeLE(city.m_ownedGuildCards);
			writeLE(city.m_ownedScienceTokens);
			writeLE(city.m_numScienceSymbols);
			writeLE(city.m_gold);
			writeLE(city.m_victoryPoints);

			for (u32 j = 0; j < u32(ScienceSymbol::Count); ++j)
				writeLE(city.m_ownedScienceSymbol[j]);

			for (u32 j = 0; j < u32(CardType::Count); ++j)
				writeLE(city.m_numCardPerType[j]);

			for (u32 j = 0; j < u32(ResourceType::Count); ++j)
				writeLE(city.m_production[j]);

			writeLE(city.m_weakProduction.first);
			writeLE(city.m_weakProduction.second);

			for (u32 j = 0; j < u32(CardType::Count); ++j)
				writeLE(u8(city.m_resourceDiscount[j] ? 1 : 0));

			for (u32 j = 0; j < u32(ResourceType::Count); ++j)
				writeLE(city.m_bestProductionCardId[j]);

			for (u32 j = 0; j < city.m_unbuildWonders.size(); ++j)
				writeLE(u8(city.m_unbuildWonders[j]));
			writeLE(city.m_unbuildWonderCount);
		}

		// Graphs per age: 3 entries
		auto writeCardNodePacked = [&](const GameState::CardNode& node)
			{
				u32 packed = 0;
				packed |= (node.m_parent0 & 0x1Fu) << 0;
				packed |= (node.m_parent1 & 0x1Fu) << 5;
				packed |= (node.m_child0 & 0x1Fu) << 10;
				packed |= (node.m_child1 & 0x1Fu) << 15;
				packed |= (node.m_cardId & 0x3FFu) << 20;
				packed |= (node.m_visible & 0x1u) << 30;
				packed |= (node.m_isGuildCard & 0x1u) << 31;
				writeLE(packed);
			};

		auto writeGraphSetup = [&](const GameState::GraphSetup& g)
			{
				// 20 nodes
				for (const auto& node : g.m_graph)
					writeCardNodePacked(node);

				// fixed-size arrays
				for (u32 i = 0; i < g.m_playableCards.size(); ++i)
					writeLE(g.m_playableCards[i]);

				for (u32 i = 0; i < g.m_availableAgeCards.size(); ++i)
					writeLE(g.m_availableAgeCards[i]);

				for (u32 i = 0; i < g.m_availableGuildCards.size(); ++i)
					writeLE(g.m_availableGuildCards[i]);

				writeLE(g.m_age);
				writeLE(g.m_numPlayableCards);
				writeLE(g.m_numAvailableAgeCards);
				writeLE(g.m_numAvailableGuildCards);
			};

		for (u32 a = 0; a < 3; ++a)
			writeGraphSetup(_state.m_graphsPerAge[a]);

		// active graph
		writeGraphSetup(_state.m_graph);

		// done
		return out;
	}

	// Must match serializeGameState() format
	// Return true on success, false on failure (bad magic/version/size)
	bool deserializeGameState(const GameContext& _context, const std::vector<u8>& _blob, GameState& _outState)
	{
		const size_t sz = _blob.size();
		size_t idx = 0;

		auto ensure = [&](size_t need) -> bool {
			return idx + need <= sz;
			};

		auto readLE_u8 = [&](u8& out) -> bool {
			if (!ensure(1)) return false;
			out = _blob[idx++];
			return true;
			};
		auto readLE_u16 = [&](u16& out) -> bool {
			if (!ensure(2)) return false;
			out = u16(_blob[idx]) | (u16(_blob[idx + 1]) << 8);
			idx += 2;
			return true;
			};
		auto readLE_u32 = [&](u32& out) -> bool {
			if (!ensure(4)) return false;
			out = u32(_blob[idx]) | (u32(_blob[idx + 1]) << 8) | (u32(_blob[idx + 2]) << 16) | (u32(_blob[idx + 3]) << 24);
			idx += 4;
			return true;
			};
		auto readLE_i8 = [&](int8_t& out) -> bool {
			u8 v;
			if (!readLE_u8(v)) return false;
			out = int8_t(v);
			return true;
			};
		auto readLE_u32_raw = [&](u32& out) -> bool { return readLE_u32(out); };

		// header: 4 bytes magic + 1 byte version
		if (!ensure(5)) return false;
		if (_blob[0] != '7' || _blob[1] != 'W' || _blob[2] != 'G' || _blob[3] != 'S') return false;
		u8 version = _blob[4];
		if (version != 3) return false; // Accept version 3 only
		idx = 5;

		// Create a fresh GameState with context so internal PlayerCity::m_context are initialized
		_outState = GameState(_context);

		// Basic scalars
		u8 tmp;
		if (!readLE_u8(tmp)) return false;
		_outState.m_state = (GameState::State)tmp;
		if (!readLE_u8(_outState.m_numTurnPlayed)) return false;
		if (!readLE_u8(_outState.m_playerTurn)) return false;
		if (!readLE_u8(_outState.m_currentAge)) return false;

		// m_military is int8_t stored as 1 byte
		{
			int8_t m;
			if (!readLE_i8(m)) return false;
			_outState.m_military = m;
		}

		// military tokens stored as u8 flags
		if (!readLE_u8(tmp)) return false; _outState.militaryToken2[0] = tmp != 0;
		if (!readLE_u8(tmp)) return false; _outState.militaryToken2[1] = tmp != 0;
		if (!readLE_u8(tmp)) return false; _outState.militaryToken5[0] = tmp != 0;
		if (!readLE_u8(tmp)) return false; _outState.militaryToken5[1] = tmp != 0;

		// science tokens
		if (!readLE_u8(_outState.m_numScienceToken)) return false;
		for (u32 i = 0; i < u32(ScienceToken::Count); ++i)
		{
			u8 v;
			if (!readLE_u8(v)) return false;
			if (i < _outState.m_numScienceToken)
				_outState.m_scienceTokens[i] = ScienceToken(v);
		}

		// played age cards
		if (!readLE_u8(_outState.m_numPlayedAgeCards)) return false;
		for (u32 i = 0; i < GameContext::MaxCardsPerAge; ++i)
		{
			if (!readLE_u8(_outState.m_playedAgeCards[i])) return false;
		}

		// DiscardedCards tracking (NEW in version 3)
		{
			auto& dc = _outState.m_discardedCards;
			
			// Production cards (5 resource types)
			for (u32 i = 0; i < u32(ResourceType::Count); ++i)
				if (!readLE_u8(dc.bestProductionCardId[i])) return false;
			
			// Single cards
			if (!readLE_u8(dc.bestBlueCardId)) return false;
			if (!readLE_u8(dc.bestMilitaryCardId)) return false;
			
			// Science cards (7 symbols)
			for (u32 i = 0; i < u32(ScienceSymbol::Count); ++i)
				if (!readLE_u8(dc.scienceCardIds[i])) return false;
			
			// Guild cards (variable count, max 7)
			if (!readLE_u8(dc.numGuildCards)) return false;
			for (u32 i = 0; i < dc.guildCardIds.size(); ++i)
				if (!readLE_u8(dc.guildCardIds[i])) return false;
			
			// Yellow cards
			if (!readLE_u8(dc.bestYellowGoldRewardCardId)) return false;
			if (!readLE_u8(dc.bestYellowWeakNormalCardId)) return false;
			if (!readLE_u8(dc.bestYellowWeakRareCardId)) return false;
			
			// Yellow resource discount cards (variable count, max 4)
			if (!readLE_u8(dc.numYellowResourceDiscountCards)) return false;
			for (u32 i = 0; i < dc.yellowResourceDiscountCardIds.size(); ++i)
				if (!readLE_u8(dc.yellowResourceDiscountCardIds[i])) return false;
			
			// Yellow gold-per-card-type cards (variable count, max 5)
			if (!readLE_u8(dc.numYellowGoldPerCardTypeCards)) return false;
			for (u32 i = 0; i < dc.yellowGoldPerCardTypeCardIds.size(); ++i)
				if (!readLE_u8(dc.yellowGoldPerCardTypeCardIds[i])) return false;
		}

		// wonder draft pool
		for (u32 i = 0; i < _outState.m_wonderDraftPool.size(); ++i)
		{
			u8 w;
			if (!readLE_u8(w)) return false;
			_outState.m_wonderDraftPool[i] = Wonders(w);
		}
		if (!readLE_u8(_outState.m_currentDraftRound)) return false;
		if (!readLE_u8(_outState.m_picksInCurrentRound)) return false;

		// player cities
		for (u32 p = 0; p < 2; ++p)
		{
			PlayerCity& city = _outState.m_playerCity[p];
			if (!readLE_u32(city.m_chainingSymbols)) return false;
			if (!readLE_u16(city.m_ownedGuildCards)) return false;
			if (!readLE_u16(city.m_ownedScienceTokens)) return false;
			if (!readLE_u8(city.m_numScienceSymbols)) return false;
			if (!readLE_u8(city.m_gold)) return false;
			if (!readLE_u8(city.m_victoryPoints)) return false;

			for (u32 j = 0; j < u32(ScienceSymbol::Count); ++j)
				if (!readLE_u8(city.m_ownedScienceSymbol[j])) return false;

			for (u32 j = 0; j < u32(CardType::Count); ++j)
				if (!readLE_u8(city.m_numCardPerType[j])) return false;

			for (u32 j = 0; j < u32(ResourceType::Count); ++j)
				if (!readLE_u8(city.m_production[j])) return false;

			if (!readLE_u8(city.m_weakProduction.first)) return false;
			if (!readLE_u8(city.m_weakProduction.second)) return false;

			for (u32 j = 0; j < u32(CardType::Count); ++j)
			{
				u8 flag;
				if (!readLE_u8(flag)) return false;
				city.m_resourceDiscount[j] = flag != 0;
			}

			for (u32 j = 0; j < u32(ResourceType::Count); ++j)
				if (!readLE_u8(city.m_bestProductionCardId[j])) return false;

			for (u32 j = 0; j < city.m_unbuildWonders.size(); ++j)
			{
				u8 w;
				if (!readLE_u8(w)) return false;
				city.m_unbuildWonders[j] = Wonders(w);
			}
			if (!readLE_u8(city.m_unbuildWonderCount)) return false;
		}

		// helper to unpack CardNode packed u32
		auto unpackCardNode = [&](const u32 packed, GameState::CardNode& node) {
			node.m_parent0 = (packed >> 0) & 0x1F;
			node.m_parent1 = (packed >> 5) & 0x1F;
			node.m_child0 = (packed >> 10) & 0x1F;
			node.m_child1 = (packed >> 15) & 0x1F;
			node.m_cardId = (packed >> 20) & 0x3FF;
			node.m_visible = (packed >> 30) & 0x1;
			node.m_isGuildCard = (packed >> 31) & 0x1;
			};

		// read a GraphSetup
		auto readGraphSetup = [&](GameState::GraphSetup& g) -> bool {
			for (auto& node : g.m_graph)
			{
				u32 packed;
				if (!readLE_u32_raw(packed)) return false;
				unpackCardNode(packed, node);
			}
			for (u32 i = 0; i < g.m_playableCards.size(); ++i)
			{
				if (!readLE_u8(g.m_playableCards[i])) return false;
			}
			for (u32 i = 0; i < g.m_availableAgeCards.size(); ++i)
			{
				if (!readLE_u8(g.m_availableAgeCards[i])) return false;
			}
			for (u32 i = 0; i < g.m_availableGuildCards.size(); ++i)
			{
				if (!readLE_u8(g.m_availableGuildCards[i])) return false;
			}
			if (!readLE_u8(g.m_age)) return false;
			if (!readLE_u8(g.m_numPlayableCards)) return false;
			if (!readLE_u8(g.m_numAvailableAgeCards)) return false;
			if (!readLE_u8(g.m_numAvailableGuildCards)) return false;
			return true;
			};

		for (u32 a = 0; a < 3; ++a)
			if (!readGraphSetup(_outState.m_graphsPerAge[a])) return false;

		if (!readGraphSetup(_outState.m_graph)) return false;

		// ensure GameContext pointers are correct (they were set by constructor, but set explicitly to be safe)
		_outState.m_context = &_context;
		for (auto& city : _outState.m_playerCity)
			city.m_context = &_context;

		// mark deterministic false (not serialized)
		_outState.m_isDeterministic = false;

		return true;
	}
}