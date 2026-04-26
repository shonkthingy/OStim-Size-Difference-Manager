#include "SceneCache/SceneCache.h"

#include "Matching/HeightMatcher.h"

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
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

void SizeDiff::SceneCache::Cache::AddExemption(std::string sceneId)
{
	std::scoped_lock lock(g_mutex);
	_exemptions.insert(ToLower(std::move(sceneId)));
}

std::vector<std::string> SizeDiff::SceneCache::Cache::GetExemptionsCopy() const
{
	std::scoped_lock lock(g_mutex);
	std::vector<std::string> out(_exemptions.begin(), _exemptions.end());
	std::ranges::sort(out);
	return out;
}

std::vector<std::pair<std::string, float>> SizeDiff::SceneCache::Cache::GetOverridesCopy() const
{
	std::scoped_lock lock(g_mutex);
	std::vector<std::pair<std::string, float>> out(_overrides.begin(), _overrides.end());
	std::ranges::sort(out, [](const auto& a, const auto& b) { return a.first < b.first; });
	return out;
}

bool SizeDiff::SceneCache::Cache::SaveUserOverrides()
{
	nlohmann::json doc;
	{
		std::scoped_lock lock(g_mutex);
		doc["exemptions"] = nlohmann::json::array();
		for (const auto& e : _exemptions) {
			doc["exemptions"].push_back(e);
		}
		doc["overrides"] = nlohmann::json::object();
		for (const auto& [k, v] : _overrides) {
			doc["overrides"][k] = v;
		}
	}

	const std::filesystem::path path{ "Data/SKSE/Plugins/OStimSizeDifferenceManager_Overrides.json" };
	std::ofstream out(path);
	if (!out.good()) {
		spdlog::error("Could not write overrides to {}", path.string());
		return false;
	}
	out << doc.dump(2);
	spdlog::info("Wrote exemptions/overrides to {}", path.string());
	return true;
}

std::map<std::string, std::vector<std::string>> SizeDiff::SceneCache::Cache::GetPackScenes() const
{
	std::map<std::string, std::vector<std::string>> out;
	{
		std::scoped_lock lock(g_mutex);
		for (const auto& [id, info] : _entries) {
			const std::string pack = info.packName.empty() ? std::string("(root)") : info.packName;
			out[pack].push_back(id);
		}
	}
	for (auto& packEntry : out) {
		std::ranges::sort(packEntry.second);
	}
	return out;
}

bool SizeDiff::SceneCache::Cache::IsExempt(const std::string& sceneId) const
{
	std::scoped_lock lock(g_mutex);
	return _exemptions.contains(ToLower(sceneId));
}

void SizeDiff::SceneCache::Cache::ToggleExemption(const std::string& sceneId, bool exempt)
{
	std::scoped_lock lock(g_mutex);
	const auto id = ToLower(sceneId);
	if (exempt) {
		_exemptions.insert(id);
	} else {
		_exemptions.erase(id);
	}
}

void SizeDiff::SceneCache::Cache::SetOverride(const std::string& sceneId, float diff)
{
	std::scoped_lock lock(g_mutex);
	_overrides[ToLower(sceneId)] = diff;
}

std::optional<SizeDiff::SceneCache::SceneScaleInfo> SizeDiff::SceneCache::Cache::GetSceneInfo(const std::string& sceneId) const
{
	std::scoped_lock lock(g_mutex);
	const auto it = _entries.find(ToLower(sceneId));
	if (it == _entries.end()) {
		return std::nullopt;
	}
	return it->second;
}

std::shared_ptr<SizeDiff::SceneCache::Cache> SizeDiff::SceneCache::Get()
{
	return g_cache;
}
