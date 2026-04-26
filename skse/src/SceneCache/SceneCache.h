#pragma once

#include <memory>
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
	};

	class Cache final
	{
	public:
		void LoadUserOverrides();
		void SetData(std::unordered_map<std::string, SceneScaleInfo> entries);
		bool IsReady() const;
		bool Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const;
		std::size_t SceneCount() const;

	private:
		std::unordered_map<std::string, SceneScaleInfo> _entries;
		std::unordered_set<std::string> _exemptions;
		std::unordered_map<std::string, float> _overrides;
		bool _ready{ false };
	};

	std::shared_ptr<Cache> Get();
}
