#include "SceneCache/SceneCache.h"

#include "Matching/HeightMatcher.h"

#include <mutex>

namespace
{
	std::mutex g_mutex;
	auto g_cache = std::make_shared<SizeDiff::SceneCache::Cache>();
}

void SizeDiff::SceneCache::Cache::SetData(std::unordered_map<std::string, SceneScaleInfo> entries)
{
	std::scoped_lock lock(g_mutex);
	_entries = std::move(entries);
	_ready = true;
}

bool SizeDiff::SceneCache::Cache::IsReady() const
{
	std::scoped_lock lock(g_mutex);
	return _ready;
}

bool SizeDiff::SceneCache::Cache::Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const
{
	std::scoped_lock lock(g_mutex);
	if (!_ready || actorScales.empty()) {
		return true;
	}

	const auto it = _entries.find(sceneId);
	if (it == _entries.end()) {
		return true;
	}

	return SizeDiff::Matching::MatchesStrict(it->second.diff, actorScales, tolerance);
}

std::size_t SizeDiff::SceneCache::Cache::SceneCount() const
{
	std::scoped_lock lock(g_mutex);
	return _entries.size();
}

std::shared_ptr<SizeDiff::SceneCache::Cache> SizeDiff::SceneCache::Get()
{
	return g_cache;
}
