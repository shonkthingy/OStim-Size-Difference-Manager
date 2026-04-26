#include "SceneCache/SceneCache.h"

#include "Matching/HeightMatcher.h"

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <ranges>

namespace
{
	std::mutex g_mutex;
	auto g_cache = std::make_shared<SizeDiff::SceneCache::Cache>();

	std::string ToLower(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}
}

void SizeDiff::SceneCache::Cache::LoadUserOverrides()
{
	std::scoped_lock lock(g_mutex);
	_exemptions.clear();
	_overrides.clear();

	const std::filesystem::path path{ "Data/SKSE/Plugins/OStimSizeDifferenceManager_Overrides.json" };
	if (!std::filesystem::exists(path)) {
		spdlog::info("OStimSizeDifferenceManager: No overrides file at {}; using built-in rules only", path.string());
		return;
	}

	std::ifstream in(path);
	if (!in.good()) {
		spdlog::warn("OStimSizeDifferenceManager: Could not open overrides file: {}", path.string());
		return;
	}

	const nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
	if (doc.is_discarded()) {
		spdlog::warn("OStimSizeDifferenceManager: Malformed JSON in {}", path.string());
		return;
	}

	if (doc.contains("exemptions") && doc["exemptions"].is_array()) {
		for (const auto& item : doc["exemptions"]) {
			if (item.is_string()) {
				_exemptions.insert(ToLower(item.get<std::string>()));
			}
		}
	}

	if (doc.contains("overrides") && doc["overrides"].is_object()) {
		for (auto it = doc["overrides"].begin(); it != doc["overrides"].end(); ++it) {
			if (!it.value().is_number()) {
				continue;
			}
			_overrides[ToLower(it.key())] = it.value().get<float>();
		}
	}

	spdlog::info("OStimSizeDifferenceManager: Loaded {} exemption(s) and {} override(s) from {}",
		_exemptions.size(), _overrides.size(), path.string());
	for (const auto& id : _exemptions) {
		spdlog::debug("  exemption: {}", id);
	}
	for (const auto& [id, diff] : _overrides) {
		spdlog::debug("  override: {} -> {}", id, diff);
	}
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

	const std::string lowercaseId = ToLower(sceneId);

	if (_exemptions.contains(lowercaseId)) {
		return true;
	}

	const auto overrideIt = _overrides.find(lowercaseId);
	if (overrideIt != _overrides.end()) {
		const float actorDiff = SizeDiff::Matching::ComputeDiff(actorScales);
		return actorDiff <= overrideIt->second + tolerance;
	}

	if (lowercaseId.starts_with("ostim")) {
		return true;
	}

	const auto it = _entries.find(lowercaseId);
	if (it == _entries.end()) {
		const float diff = SizeDiff::Matching::ComputeDiff(actorScales);
		return diff <= tolerance;
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
