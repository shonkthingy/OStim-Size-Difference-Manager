#pragma once

#include <memory>
#include <string>
#include <unordered_map>
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
		void SetData(std::unordered_map<std::string, SceneScaleInfo> entries);
		bool IsReady() const;
		bool Matches(const std::string& sceneId, const std::vector<float>& actorScales, float tolerance) const;
		std::size_t SceneCount() const;

	private:
		std::unordered_map<std::string, SceneScaleInfo> _entries;
		bool _ready{ false };
	};

	std::shared_ptr<Cache> Get();
}
