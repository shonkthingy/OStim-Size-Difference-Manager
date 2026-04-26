#pragma once

#include "OStimTypes/Node.h"

#include <string>
#include <vector>

namespace Graph
{
	/// Layout must match OStim `Graph::Navigation` (see OStimNG Graph/Node.h). We only use `nodes` in hooks.
	struct Navigation
	{
		std::vector<Node*> nodes;
		std::string description;
		std::string icon;
		std::string border;
		bool isTransition{ false };
	};
}
