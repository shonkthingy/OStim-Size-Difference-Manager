#pragma once

#include <set>
#include <string>

namespace GameAPI
{
	enum GameSex
	{
		MALE,
		FEMALE,
		AGENDER
	};
}

namespace Trait
{
	struct ActorCondition
	{
		std::string type = "npc";
		GameAPI::GameSex sex = GameAPI::GameSex::AGENDER;
		std::set<std::string> requirements;
	};
}
