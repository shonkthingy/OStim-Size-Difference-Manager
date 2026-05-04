#pragma once

#include <optional>
#include <string>

namespace SizeDiff::AddressResolution
{
	std::optional<std::string> GetOStimVersionString();
	bool IsKnownGoodVersion(const std::string& version);

	// Pre-7.3.5.3 PEs use different getRandomNode / getRandomNodeInRange prologues (see OStimSizeDifferenceManager-Signatures.json).
	bool UsesLegacyGraphBytePatterns(const std::string& version);
}
