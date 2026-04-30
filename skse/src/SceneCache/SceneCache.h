#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SizeDiff::SceneCache
{
	struct SceneScaleInfo
	{
		float minScale{ 1.0F };
		float maxScale{ 1.0F };
		float diff{ 0.0F };
		int actorCount{ 0 };
		bool hasMissingActorScale{ false };
		std::string packName;  // top-level folder under scenes/, e.g. "AnubsPack", "BakaHuman"
	};

	struct UiSnapshot
	{
		std::map<std::string, std::vector<std::string>> packScenes;
		std::unordered_map<std::string, SceneScaleInfo> entries;
		std::unordered_set<std::string> exemptions;
		std::unordered_set<std::string> exemptPacks;
		std::unordered_map<std::string, float> overrides;
		std::vector<std::string> unindexedJsonSceneIds;
		std::uint64_t revision{ 0 };
	};

	enum class PersistStatus : std::uint8_t
	{
		Saved = 0,
		Dirty = 1,
		SaveFailed = 2
	};

	class Cache final
	{
	public:
		void LoadUserOverrides();
		void SetData(std::unordered_map<std::string, SceneScaleInfo> entries);
		bool IsReady() const;
		bool Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const;
		// Absolute scale-diff distance from actor targets for soft-mode ordering (0 = best; exemptions/overrides/unknown handled like Matches)
		float SoftDistanceFromActors(const std::string& sceneId, const std::vector<float>& actorScales) const;
		std::size_t SceneCount() const;

		void AddExemption(std::string sceneId);
		std::vector<std::string> GetExemptionsCopy() const;
		std::vector<std::string> GetExemptPacksCopy() const;
		std::vector<std::pair<std::string, float>> GetOverridesCopy() const;
		std::vector<std::string> GetUnindexedJsonSceneIds() const;
		bool SaveUserOverrides();
		UiSnapshot GetUiSnapshot() const;
		std::uint64_t GetRevision() const;
		bool FlushDirtyNow();
		bool TryAutosave(std::chrono::steady_clock::time_point now, std::chrono::milliseconds debounce);
		PersistStatus GetPersistStatus() const;
		bool HasUnsavedChanges() const;

		// Returns map of pack name -> scene IDs in that pack (sorted). Thread-safe.
		std::map<std::string, std::vector<std::string>> GetPackScenes() const;

		bool IsExempt(const std::string& sceneId) const;
		bool IsPackExempt(const std::string& packName) const;
		bool IsEffectivelyExempt(const std::string& sceneId) const;
		void ToggleExemption(const std::string& sceneId, bool exempt);
		void TogglePackExemption(const std::string& packName, bool exempt);
		void SetOverride(const std::string& sceneId, float diff);
		std::optional<SceneScaleInfo> GetSceneInfo(const std::string& sceneId) const;

	private:
		static std::vector<std::string> BuildUnindexedJsonSceneIds(
			const std::unordered_set<std::string>& exemptions,
			const std::unordered_map<std::string, float>& overrides,
			const std::unordered_map<std::string, SceneScaleInfo>& entries);
		void MarkDirtyLocked();
		std::unordered_map<std::string, SceneScaleInfo> _entries;
		std::unordered_set<std::string> _exemptions;
		std::unordered_set<std::string> _exemptPacks;
		std::unordered_map<std::string, float> _overrides;
		std::uint64_t _revision{ 0 };
		bool _dirty{ false };
		bool _saveFailed{ false };
		std::chrono::steady_clock::time_point _lastMutation{};
		std::chrono::steady_clock::time_point _lastAutosaveAttempt{};
		bool _ready{ false };
	};

	std::shared_ptr<Cache> Get();
}
