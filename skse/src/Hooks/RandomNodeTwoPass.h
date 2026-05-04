#pragma once

#include "Config/Config.h"
#include "OStimTypes/ActorCondition.h"
#include "OStimTypes/Node.h"
#include "SceneCache/SceneCache.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <utility>
#include <vector>

namespace SizeDiff::Hooks::RandomNodeTwoPass
{
	/// Two-pass: strict @a tolerance, then soft gather (closest scale-diff) when mode is Soft.
	/// @param callOriginal Invokes the hooked OStim function with (moved) vector and predicate.
	template <typename CallOriginal>
	Graph::Node* Run(
		CallOriginal&& callOriginal,
		std::vector<Trait::ActorCondition> actorConditions,
		std::function<bool(Graph::Node*)> nodeCondition,
		const std::vector<float>& scales,
		const std::shared_ptr<SceneCache::Cache>& cache,
		const char* logTag)
	{
		const auto settings = Config::Get();
		const float tolerance = settings.tolerance;
		const std::vector<Trait::ActorCondition> acCopy = actorConditions;
		const std::function<bool(Graph::Node*)> nodePredCopy = nodeCondition;

		{
			std::vector<Trait::ActorCondition> ac1 = std::move(actorConditions);
			std::function<bool(Graph::Node*)> nc1 = std::move(nodeCondition);
			const auto strictPred = [orig = std::move(nc1), cache, scales, tolerance](Graph::Node* node) -> bool {
				if (orig && !orig(node)) {
					return false;
				}
				if (!node) {
					return false;
				}
				return cache->Matches(node->getNodeID(), scales, tolerance);
			};

			Graph::Node* strictResult = callOriginal(std::move(ac1), std::move(strictPred));
			if (strictResult) {
				const char* const strictNodeId = strictResult->getNodeID();
				spdlog::trace("{}: Strict Match -> {}", logTag, strictNodeId ? strictNodeId : "<null>");
				return strictResult;
			}
		}

		if (settings.mode == Config::Mode::Debug) {
			spdlog::debug("{}: No strict match; allowing unfiltered node (debug)", logTag);
			return callOriginal(std::vector<Trait::ActorCondition>(acCopy), std::function<bool(Graph::Node*)>(nodePredCopy));
		}

		if (settings.mode != Config::Mode::Soft) {
			spdlog::debug("{}: No Match (strict pass did not find a node; mode is not Soft)", logTag);
			return nullptr;
		}

		std::vector<std::pair<Graph::Node*, float>> evaluatedNodes;
		{
			std::vector<Trait::ActorCondition> ac2 = acCopy;
			std::function<bool(Graph::Node*)> nc2 = nodePredCopy;
			const auto gatherPred = [orig = std::move(nc2), cache, scales, &evaluatedNodes](Graph::Node* node) -> bool {
				if (orig && !orig(node)) {
					return false;
				}
				if (!node) {
					return false;
				}
				const float d = cache->SoftDistanceFromActors(node->getNodeID(), scales);
				evaluatedNodes.push_back({ node, d });
				return false;
			};

			callOriginal(std::move(ac2), std::move(gatherPred));
		}

		if (evaluatedNodes.empty()) {
			spdlog::debug("{}: No Match (soft gather: no node passed original + graph predicate)", logTag);
			return nullptr;
		}

		constexpr float kEps = 0.001F;
		std::sort(evaluatedNodes.begin(), evaluatedNodes.end(),
			[](const std::pair<Graph::Node*, float>& a, const std::pair<Graph::Node*, float>& b) {
				return a.second < b.second;
			});

		const float best = evaluatedNodes.front().second;
		std::vector<Graph::Node*> tied;
		for (const auto& p : evaluatedNodes) {
			if (std::abs(p.second - best) <= kEps) {
				tied.push_back(p.first);
			} else {
				break;
			}
		}

		if (tied.empty()) {
			spdlog::debug("{}: soft tie-break produced no candidate nodes", logTag);
			return nullptr;
		}

		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_int_distribution<std::size_t> dist(0, tied.size() - 1);
		const std::size_t idx = dist(rng);
		Graph::Node* const pick = tied[idx];
		const char* const pickedNodeId = pick ? pick->getNodeID() : nullptr;
		spdlog::debug("{}: Soft Match -> {} (delta={})", logTag, pickedNodeId ? pickedNodeId : "<null>", best);
		return pick;
	}
}
