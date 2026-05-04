#include "SceneCache/SceneCache.h"

#include "Matching/HeightMatcher.h"
#include "Util/Log.h"

#include <cmath>

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>

namespace
{
	std::mutex g_mutex;
	auto g_cache = std::make_shared<SizeDiff::SceneCache::Cache>();
	const std::filesystem::path g_overridesPath{ "Data/SKSE/Plugins/OStimSizeDifferenceManager_Overrides.json" };
	std::atomic_uint32_t g_overrideSaveRetryCount{ 0 };

	std::string ToLower(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}
}

std::vector<std::string> SizeDiff::SceneCache::Cache::BuildUnindexedJsonSceneIds(
	const std::unordered_set<std::string>& exemptions,
	const std::unordered_map<std::string, float>& overrides,
	const std::unordered_map<std::string, SceneScaleInfo>& entries)
{
	std::unordered_set<std::string> ids;
	ids.reserve(exemptions.size() + overrides.size());
	for (const auto& id : exemptions) {
		if (!entries.contains(id)) {
			ids.insert(id);
		}
	}
	for (const auto& [id, _] : overrides) {
		if (!entries.contains(id)) {
			ids.insert(id);
		}
	}

	std::vector<std::string> out(ids.begin(), ids.end());
	std::ranges::sort(out);
	return out;
}

void SizeDiff::SceneCache::Cache::MarkDirtyLocked()
{
	_dirty = true;
	_saveFailed = false;
	_lastMutation = std::chrono::steady_clock::now();
	++_revision;
}

void SizeDiff::SceneCache::Cache::LoadUserOverrides()
{
	std::scoped_lock lock(g_mutex);
	_exemptions.clear();
	_exemptPacks.clear();
	_overrides.clear();
	++_revision;
	_dirty = false;
	_saveFailed = false;

	const std::filesystem::path& path = g_overridesPath;
	if (!std::filesystem::exists(path)) {
		spdlog::debug("OStimSizeDifferenceManager: No overrides file at {}; using built-in rules only", path.string());
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
	if (doc.contains("exemptPacks") && doc["exemptPacks"].is_array()) {
		for (const auto& item : doc["exemptPacks"]) {
			if (item.is_string()) {
				_exemptPacks.insert(ToLower(item.get<std::string>()));
			}
		}
	}

	if (doc.contains("overrides") && doc["overrides"].is_object()) {
		for (auto it = doc["overrides"].begin(); it != doc["overrides"].end(); ++it) {
			if (!it.value().is_number()) {
				spdlog::warn("OStimSizeDifferenceManager: Ignoring non-numeric override '{}' in {}", it.key(), path.string());
				continue;
			}
			_overrides[ToLower(it.key())] = it.value().get<float>();
		}
	}

	spdlog::info("OStimSizeDifferenceManager: Loaded {} scene exemption(s), {} pack exemption(s), and {} override(s) from {}",
		_exemptions.size(), _exemptPacks.size(), _overrides.size(), path.string());
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
	++_revision;
}

bool SizeDiff::SceneCache::Cache::IsReady() const
{
	std::scoped_lock lock(g_mutex);
	return _ready;
}

bool SizeDiff::SceneCache::Cache::Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const
{
	std::scoped_lock lock(g_mutex);
	if (!_ready) {
		if (SizeDiff::Log::ShouldLogNow("scenecache_matches_not_ready", std::chrono::milliseconds(5000))) {
			spdlog::trace("SceneCache::Matches bypassed because cache is not ready (sceneId='{}')", sceneId);
		}
		return true;
	}
	if (actorScales.empty()) {
		if (SizeDiff::Log::ShouldLogNow("scenecache_matches_empty_scales", std::chrono::milliseconds(5000))) {
			spdlog::trace("SceneCache::Matches bypassed because actor scales are empty (sceneId='{}')", sceneId);
		}
		return true;
	}

	const std::string lowercaseId = ToLower(sceneId);

	if (_exemptions.contains(lowercaseId)) {
		return true;
	}
	const auto infoIt = _entries.find(lowercaseId);
	if (infoIt != _entries.end() && _exemptPacks.contains(ToLower(infoIt->second.packName))) {
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

	if (infoIt == _entries.end()) {
		const float diff = SizeDiff::Matching::ComputeDiff(actorScales);
		return diff <= tolerance;
	}

	return SizeDiff::Matching::MatchesStrict(infoIt->second.diff, actorScales, tolerance);
}

float SizeDiff::SceneCache::Cache::SoftDistanceFromActors(const std::string& sceneId, const std::vector<float>& actorScales) const
{
	std::scoped_lock lock(g_mutex);
	if (actorScales.empty()) {
		return 0.0F;
	}

	const std::string lowercaseId = ToLower(sceneId);

	if (_exemptions.contains(lowercaseId)) {
		return 0.0F;
	}
	const auto infoIt = _entries.find(lowercaseId);
	if (infoIt != _entries.end() && _exemptPacks.contains(ToLower(infoIt->second.packName))) {
		return 0.0F;
	}

	const auto overrideIt = _overrides.find(lowercaseId);
	const float actorDiff = SizeDiff::Matching::ComputeDiff(actorScales);
	if (overrideIt != _overrides.end()) {
		return std::abs(actorDiff - overrideIt->second);
	}

	if (lowercaseId.starts_with("ostim")) {
		return 0.0F;
	}

	if (infoIt == _entries.end()) {
		return 0.0F;
	}

	return std::abs(actorDiff - infoIt->second.diff);
}

std::size_t SizeDiff::SceneCache::Cache::SceneCount() const
{
	std::scoped_lock lock(g_mutex);
	return _entries.size();
}

void SizeDiff::SceneCache::Cache::AddExemption(std::string sceneId)
{
	std::scoped_lock lock(g_mutex);
	if (_exemptions.insert(ToLower(std::move(sceneId))).second) {
		MarkDirtyLocked();
	}
}

std::vector<std::string> SizeDiff::SceneCache::Cache::GetExemptionsCopy() const
{
	std::scoped_lock lock(g_mutex);
	std::vector<std::string> out(_exemptions.begin(), _exemptions.end());
	std::ranges::sort(out);
	return out;
}

std::vector<std::string> SizeDiff::SceneCache::Cache::GetExemptPacksCopy() const
{
	std::scoped_lock lock(g_mutex);
	std::vector<std::string> out(_exemptPacks.begin(), _exemptPacks.end());
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

std::vector<std::string> SizeDiff::SceneCache::Cache::GetUnindexedJsonSceneIds() const
{
	std::scoped_lock lock(g_mutex);
	return BuildUnindexedJsonSceneIds(_exemptions, _overrides, _entries);
}

bool SizeDiff::SceneCache::Cache::SaveUserOverrides()
{
	nlohmann::json doc;
	bool hadDirty = false;
	{
		std::scoped_lock lock(g_mutex);
		hadDirty = _dirty;
		doc["exemptions"] = nlohmann::json::array();
		for (const auto& e : _exemptions) {
			doc["exemptions"].push_back(e);
		}
		doc["exemptPacks"] = nlohmann::json::array();
		for (const auto& pack : _exemptPacks) {
			doc["exemptPacks"].push_back(pack);
		}
		doc["overrides"] = nlohmann::json::object();
		for (const auto& [k, v] : _overrides) {
			doc["overrides"][k] = v;
		}
	}

	const std::filesystem::path& path = g_overridesPath;
	const std::filesystem::path tmpPath = path.string() + ".tmp";
	std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
	if (!out.good()) {
		spdlog::error("Could not write overrides to {}", path.string());
		std::scoped_lock lock(g_mutex);
		_saveFailed = true;
		return false;
	}
	out << doc.dump(2);
	out.flush();
	if (!out.good()) {
		spdlog::error("Could not flush overrides temp file {}", tmpPath.string());
		std::scoped_lock lock(g_mutex);
		_saveFailed = true;
		return false;
	}
	out.close();

	std::error_code ec;
	std::filesystem::rename(tmpPath, path, ec);
	if (ec) {
		std::filesystem::remove(path, ec);
		ec.clear();
		std::filesystem::rename(tmpPath, path, ec);
		if (ec) {
			spdlog::error("Could not atomically replace overrides file {}: {}", path.string(), ec.message());
			std::scoped_lock lock(g_mutex);
			_saveFailed = true;
			return false;
		}
	}
	spdlog::info("Wrote exemptions/overrides to {}", path.string());
	if (hadDirty) {
		std::scoped_lock lock(g_mutex);
		_dirty = false;
		_saveFailed = false;
	}
	return true;
}

SizeDiff::SceneCache::UiSnapshot SizeDiff::SceneCache::Cache::GetUiSnapshot() const
{
	std::scoped_lock lock(g_mutex);

	UiSnapshot snap;
	snap.revision = _revision;
	snap.entries = _entries;
	snap.exemptions = _exemptions;
	snap.exemptPacks = _exemptPacks;
	snap.overrides = _overrides;
	snap.unindexedJsonSceneIds = BuildUnindexedJsonSceneIds(_exemptions, _overrides, _entries);

	for (const auto& [id, info] : _entries) {
		const std::string pack = info.packName.empty() ? std::string("(root)") : info.packName;
		snap.packScenes[pack].push_back(id);
	}
	for (auto& [_, sceneIds] : snap.packScenes) {
		std::ranges::sort(sceneIds);
	}

	return snap;
}

std::uint64_t SizeDiff::SceneCache::Cache::GetRevision() const
{
	std::scoped_lock lock(g_mutex);
	return _revision;
}

bool SizeDiff::SceneCache::Cache::FlushDirtyNow()
{
	return SaveUserOverrides();
}

bool SizeDiff::SceneCache::Cache::TryAutosave(std::chrono::steady_clock::time_point now, std::chrono::milliseconds debounce)
{
	{
		std::scoped_lock lock(g_mutex);
		if (!_dirty) {
			return true;
		}
		if (now - _lastMutation < debounce || now - _lastAutosaveAttempt < debounce) {
			return false;
		}
		_lastAutosaveAttempt = now;
	}
	const bool saved = SaveUserOverrides();
	if (!saved && SizeDiff::Log::ShouldLogNow("scenecache_autosave_retry", std::chrono::milliseconds(10000))) {
		const auto attempt = g_overrideSaveRetryCount.fetch_add(1) + 1;
		spdlog::warn(
			"[OVERRIDE_SAVE_RETRY] attempt={} path={} sinceFirstFailureMs={} lastError=persist_write_failed",
			attempt,
			g_overridesPath.string(),
			std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastMutation).count());
	}
	if (saved) {
		g_overrideSaveRetryCount.store(0);
	}
	return saved;
}

SizeDiff::SceneCache::PersistStatus SizeDiff::SceneCache::Cache::GetPersistStatus() const
{
	std::scoped_lock lock(g_mutex);
	if (_saveFailed) {
		return PersistStatus::SaveFailed;
	}
	return _dirty ? PersistStatus::Dirty : PersistStatus::Saved;
}

bool SizeDiff::SceneCache::Cache::HasUnsavedChanges() const
{
	std::scoped_lock lock(g_mutex);
	return _dirty;
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

bool SizeDiff::SceneCache::Cache::IsPackExempt(const std::string& packName) const
{
	std::scoped_lock lock(g_mutex);
	return _exemptPacks.contains(ToLower(packName));
}

bool SizeDiff::SceneCache::Cache::IsEffectivelyExempt(const std::string& sceneId) const
{
	std::scoped_lock lock(g_mutex);
	const auto id = ToLower(sceneId);
	if (_exemptions.contains(id)) {
		return true;
	}
	const auto it = _entries.find(id);
	if (it == _entries.end()) {
		return false;
	}
	return _exemptPacks.contains(ToLower(it->second.packName));
}

void SizeDiff::SceneCache::Cache::ToggleExemption(const std::string& sceneId, bool exempt)
{
	std::scoped_lock lock(g_mutex);
	const auto id = ToLower(sceneId);
	const bool wasExempt = _exemptions.contains(id);
	if (exempt) {
		if (_exemptions.insert(id).second) {
			MarkDirtyLocked();
			spdlog::info("[EXEMPTION_CHANGED] source=ui scope=scene id={} oldState={} newState={}", id, wasExempt, true);
		}
	} else {
		if (_exemptions.erase(id) > 0) {
			MarkDirtyLocked();
			spdlog::info("[EXEMPTION_CHANGED] source=ui scope=scene id={} oldState={} newState={}", id, wasExempt, false);
		}
	}
}

void SizeDiff::SceneCache::Cache::TogglePackExemption(const std::string& packName, bool exempt)
{
	std::scoped_lock lock(g_mutex);
	const auto pack = ToLower(packName);
	const bool wasExempt = _exemptPacks.contains(pack);
	if (exempt) {
		if (_exemptPacks.insert(pack).second) {
			MarkDirtyLocked();
			spdlog::info("[EXEMPTION_CHANGED] source=ui scope=pack id={} oldState={} newState={}", pack, wasExempt, true);
		}
	} else {
		if (_exemptPacks.erase(pack) > 0) {
			MarkDirtyLocked();
			spdlog::info("[EXEMPTION_CHANGED] source=ui scope=pack id={} oldState={} newState={}", pack, wasExempt, false);
		}
	}
}

void SizeDiff::SceneCache::Cache::SetOverride(const std::string& sceneId, float diff)
{
	std::scoped_lock lock(g_mutex);
	const auto id = ToLower(sceneId);
	const auto it = _overrides.find(id);
	const float oldValue = (it != _overrides.end()) ? it->second : 0.0F;
	if (it != _overrides.end() && std::fabs(it->second - diff) <= 1e-6F) {
		return;
	}
	_overrides[id] = diff;
	MarkDirtyLocked();
	spdlog::debug(
		"[OVERRIDE_CHANGED] source=cache sceneId={} oldValue={} newValue={} action=set_override",
		id,
		oldValue,
		diff);
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
