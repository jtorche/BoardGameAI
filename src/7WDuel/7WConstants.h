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
		Piraeus,
		Pyramids,
		Sphinx,
		Zeus,
		Atremis,
		ViaAppia,
		Mausoleum,
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

		FirstYellow = Jar,
		LastYellow = Barrel,

		FirstBlue = Mask,
		LastBlue = Moon,

		FirstRed = Target,
		LastRed = Tower,

		FirstGreen = Harp,
		LastGreen = Lamp,

		Count,
	};

	template<CardType T>
	struct CardTag {};

	enum class ScienceToken : u8
	{
		Strategy,
		Masonry,
		Economy,
		Mathematics,
		TownPlanning,

		Theology,
		Law,
		Architecture,
		Philosophy,
		Agriculture,
		Count,
		CountForNN = Theology, // we do not save token's bellow in the NN input (per player city)
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
