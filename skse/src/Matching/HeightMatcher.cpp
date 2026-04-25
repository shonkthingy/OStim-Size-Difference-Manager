#include "Matching/HeightMatcher.h"

#include <algorithm>
#include <cmath>

float SizeDiff::Matching::ComputeDiff(const std::vector<float>& scales)
{
	if (scales.empty()) {
		return 0.0F;
	}
	const auto [minIt, maxIt] = std::minmax_element(scales.begin(), scales.end());
	return *maxIt - *minIt;
}

bool SizeDiff::Matching::MatchesStrict(float sceneDiff, const std::vector<float>& actorScales, float tolerance)
{
	const auto actorDiff = ComputeDiff(actorScales);
	return std::abs(actorDiff - sceneDiff) <= tolerance;
}
