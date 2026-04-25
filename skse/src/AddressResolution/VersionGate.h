#pragma once

#include <optional>
#include <string>

namespace SizeDiff::AddressResolution
{
	std::optional<std::string> GetOStimVersionString();
	bool IsKnownGoodVersion(const std::string& version);
}
