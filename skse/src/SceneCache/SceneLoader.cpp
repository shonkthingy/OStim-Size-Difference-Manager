#include "SceneCache/SceneLoader.h"

#include "SceneCache/SceneCache.h"
#include "Util/Log.h"

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
	struct ScanResult
	{
		std::unordered_map<std::string, SizeDiff::SceneCache::SceneScaleInfo> entries;
		std::size_t jsonFileCount{ 0 };
		std::size_t indexedSceneCount{ 0 };
		std::size_t malformedJsonCount{ 0 };
		std::size_t unreadableFileCount{ 0 };
		std::size_t missingActorsArrayCount{ 0 };
		std::size_t missingActorScaleCount{ 0 };
	};

	std::string ToLower(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	ScanResult LoadSceneMetadata()
	{
		ScanResult result;
		const std::filesystem::path root{ "Data/SKSE/Plugins/OStim/scenes" };
		constexpr std::size_t kDetailedWarnCap = 8;

		if (!std::filesystem::exists(root)) {
			spdlog::warn("Scene path missing: {}", root.string());
			return result;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") {
				continue;
			}
			++result.jsonFileCount;

			std::ifstream in(entry.path());
			if (!in.good()) {
				++result.unreadableFileCount;
				if (result.unreadableFileCount <= kDetailedWarnCap) {
					spdlog::warn("Skipping unreadable scene json: {}", entry.path().string());
				}
				continue;
			}

			json doc = json::parse(in, nullptr, false);
			if (doc.is_discarded()) {
				++result.malformedJsonCount;
				if (result.malformedJsonCount <= kDetailedWarnCap) {
					spdlog::warn("Skipping malformed scene json: {}", entry.path().string());
				}
				continue;
			}

			std::string sceneId = ToLower(entry.path().stem().string());
			if (doc.contains("id") && doc["id"].is_string()) {
				sceneId = ToLower(doc["id"].get<std::string>());
			}

			if (!doc.contains("actors") || !doc["actors"].is_array()) {
				++result.missingActorsArrayCount;
				if (result.missingActorsArrayCount <= kDetailedWarnCap) {
					spdlog::debug("Skipping scene json with no actors array: {}", entry.path().string());
				}
				continue;
			}

			float minScale = std::numeric_limits<float>::max();
			float maxScale = std::numeric_limits<float>::lowest();
			int count = 0;
			bool hasMissingActorScale = false;
			for (const auto& actor : doc["actors"]) {
				float scale = 1.0F;
				if (actor.is_object() && actor.contains("scale") && actor["scale"].is_number()) {
					scale = actor["scale"].get<float>();
				} else {
					hasMissingActorScale = true;
				}
				minScale = std::min(minScale, scale);
				maxScale = std::max(maxScale, scale);
				++count;
			}

			if (count > 0) {
				std::string packName;
				const auto rel = std::filesystem::relative(entry.path(), root);
				std::vector<std::filesystem::path> components;
				for (const auto& part : rel) {
					components.push_back(part);
				}
				// rel is like pack/foo/bar.json — first path component is the pack folder
				if (components.size() >= 2) {
					packName = components.front().string();
				}

				result.entries[sceneId] = SizeDiff::SceneCache::SceneScaleInfo{
					.minScale = minScale,
					.maxScale = maxScale,
					.diff = maxScale - minScale,
					.actorCount = count,
					.hasMissingActorScale = hasMissingActorScale,
					.packName = std::move(packName)
				};
				++result.indexedSceneCount;
				if (hasMissingActorScale) {
					++result.missingActorScaleCount;
				}
			}
		}

		return result;
	}
}

void SizeDiff::SceneCache::StartBackgroundScan()
{
	std::thread([] {
		try {
			SizeDiff::SceneCache::Get()->LoadUserOverrides();
			auto scan = LoadSceneMetadata();
			SizeDiff::SceneCache::Get()->SetData(std::move(scan.entries));
			spdlog::info(
				"[SCENE_SCAN_SUMMARY] indexed={} jsonFiles={} malformed={} unreadable={} missingActorsArray={} missingActorScale={} malformedSuppressed={} unreadableSuppressed={}",
				scan.indexedSceneCount,
				scan.jsonFileCount,
				scan.malformedJsonCount,
				scan.unreadableFileCount,
				scan.missingActorsArrayCount,
				scan.missingActorScaleCount,
				scan.malformedJsonCount > 8 ? (scan.malformedJsonCount - 8) : 0,
				scan.unreadableFileCount > 8 ? (scan.unreadableFileCount - 8) : 0);
		} catch (const std::exception& e) {
			spdlog::error("[SCENE_SCAN_FAIL] error={}", e.what());
		} catch (...) {
			spdlog::error("[SCENE_SCAN_FAIL] error=unknown_exception");
		}
	}).detach();
}
