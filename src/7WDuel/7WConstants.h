#pragma once

#include "Core/type.h"

namespace sevenWD
{
	enum class ResourceType : u32
	{
		Wood = 0,
		Clay,
		Stone,
		Glass,
		Papyrus,
		Count,
		FirstBrown = Wood,
		LastBrown = Stone,
		FirstGrey = Glass,
		LastGrey = Papyrus
	};

	using RT = ResourceType;

	using ResourceSet = std::initializer_list<ResourceType>;

	enum class CardType : u8
	{
		Blue = 0,
		Brown,
		Grey,
		Yellow,
		Science,
		Military,
		Guild,
		Wonder,
		ScienceToken,
		Count
	};

	enum class Wonders : u8
	{
		CircusMaximus,
		Coloss,
		GreatLighthouse,
		HangingGarden,
		GreatLibrary,
		Mausoleum,
		Piraeus,
		Pyramids,
		Sphinx,
		Zeus,
		Atremis,
		ViaAppia,
		Count
	};

	enum class ScienceSymbol : u8
	{
		Wheel,
		Script,
		Triangle,
		Bowl,
		SolarClock,
		Globe,
		Law,
		Count
	};

	enum class ChainingSymbol : u8
	{
		None,
		Jar,
		Barrel,
		Mask,
		Bank,
		Sun,
		WaterDrop,
		GreekPillar,
		Moon,
		Target,
		Helmet,
		Horseshoe,
		Sword,
		Tower,
		Harp,
		Gear,
		Book,
		Lamp,
		Count,
	};

	template<CardType T>
	struct CardTag {};

	enum class ScienceToken : u8
	{
		Agriculture = 0,
		TownPlanning,
		Architecture,
		Theology,
		Strategy,
		Law,
		Mathematics,
		Masonry,
		Philosophy,
		Economy,
		Count
	};

	enum class SpecialAction
	{
		Nothing,
		Replay,
		TakeScienceToken,
		MilitaryWin,
		ScienceWin
	};
}
