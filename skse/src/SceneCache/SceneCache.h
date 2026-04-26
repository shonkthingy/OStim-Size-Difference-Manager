#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
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
		std::string packName;  // top-level folder under scenes/, e.g. "AnubsPack", "BakaHuman"
	};

	class Cache final
	{
	public:
		void LoadUserOverrides();
		void SetData(std::unordered_map<std::string, SceneScaleInfo> entries);
		bool IsReady() const;
		bool Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const;
		std::size_t SceneCount() const;

		void AddExemption(std::string sceneId);
		std::vector<std::string> GetExemptionsCopy() const;
		std::vector<std::pair<std::string, float>> GetOverridesCopy() const;
		bool SaveUserOverrides();

		// Returns map of pack name -> scene IDs in that pack (sorted). Thread-safe.
		std::map<std::string, std::vector<std::string>> GetPackScenes() const;

		bool IsExempt(const std::string& sceneId) const;
		void ToggleExemption(const std::string& sceneId, bool exempt);
		void SetOverride(const std::string& sceneId, float diff);
		std::optional<SceneScaleInfo> GetSceneInfo(const std::string& sceneId) const;

	private:
		std::unordered_map<std::string, SceneScaleInfo> _entries;
		std::unordered_set<std::string> _exemptions;
		std::unordered_map<std::string, float> _overrides;
		bool _ready{ false };
	};

	std::shared_ptr<Cache> Get();
}
