#pragma once

#include <optional>
#include <string>

namespace SizeDiff::AddressResolution
{
	struct PatternDefinition
	{
		std::string version;
		std::string pattern;
	};

	std::optional<std::uintptr_t> ResolveByPattern(const PatternDefinition& definition);
}
