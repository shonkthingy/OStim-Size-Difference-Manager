#include "SceneCache/SceneLoader.h"

#include "SceneCache/SceneCache.h"

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ranges>
#include <thread>

using json = nlohmann::json;

namespace
{
	std::string ToLower(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	std::unordered_map<std::string, SizeDiff::SceneCache::SceneScaleInfo> LoadSceneMetadata()
	{
		std::unordered_map<std::string, SizeDiff::SceneCache::SceneScaleInfo> result;
		const std::filesystem::path root{ "Data/SKSE/Plugins/OStim/scenes" };

		if (!std::filesystem::exists(root)) {
			spdlog::warn("Scene path missing: {}", root.string());
			return result;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") {
				continue;
			}

			std::ifstream in(entry.path());
			if (!in.good()) {
				continue;
			}

			json doc = json::parse(in, nullptr, false);
			if (doc.is_discarded()) {
				spdlog::warn("Skipping malformed scene json: {}", entry.path().string());
				continue;
			}

			std::string sceneId = ToLower(entry.path().stem().string());
			if (doc.contains("id") && doc["id"].is_string()) {
				sceneId = ToLower(doc["id"].get<std::string>());
			}

			if (!doc.contains("actors") || !doc["actors"].is_array()) {
				continue;
			}

			float minScale = std::numeric_limits<float>::max();
			float maxScale = std::numeric_limits<float>::lowest();
			int count = 0;
			for (const auto& actor : doc["actors"]) {
				float scale = 1.0F;
				if (actor.is_object() && actor.contains("scale") && actor["scale"].is_number()) {
					scale = actor["scale"].get<float>();
				}
				minScale = std::min(minScale, scale);
				maxScale = std::max(maxScale, scale);
				++count;
			}

			if (count > 0) {
				result[sceneId] = SizeDiff::SceneCache::SceneScaleInfo{
					.minScale = minScale,
					.maxScale = maxScale,
					.diff = maxScale - minScale,
					.actorCount = count
				};
			}
		}

		return result;
	}
}

void SizeDiff::SceneCache::StartBackgroundScan()
{
	std::thread([] {
		SizeDiff::SceneCache::Get()->LoadUserOverrides();
		auto data = LoadSceneMetadata();
		const auto total = data.size();
		SizeDiff::SceneCache::Get()->SetData(std::move(data));
		spdlog::info("Scene metadata scan complete: {} scenes indexed", total);
	}).detach();
}
