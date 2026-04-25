#pragma once

#include <vector>

namespace SizeDiff::Matching
{
	float ComputeDiff(const std::vector<float>& scales);
	bool MatchesStrict(float sceneDiff, const std::vector<float>& actorScales, float tolerance);
}
