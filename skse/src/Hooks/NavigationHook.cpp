#include "Hooks/NavigationHook.h"

#include "AddressResolution/PatternScanner.h"
#include "AddressResolution/PdbResolver.h"
#include "AddressResolution/VersionGate.h"
#include "Config/Config.h"
#include "Hooks/FilterContext.h"
#include "OStimTypes/ActorCondition.h"
#include "OStimTypes/Node.h"
#include "SceneCache/SceneCache.h"
#include "Util/State.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

namespace
{
	// x64: RCX = this, RDX = distance, R8/R9 = vector + function args per MSVC ABI
	using GetRandomNodeInRange_t = Graph::Node* (*)(
		Graph::Node* self,
		int distance,
		std::vector<Trait::ActorCondition> actorConditions,
		std::function<bool(Graph::Node*)> nodeCondition);

	GetRandomNodeInRange_t g_originalGetRandomNodeInRange{ nullptr };

	Graph::Node* HookedGetRandomNodeInRange(
		Graph::Node* self,
		int distance,
		std::vector<Trait::ActorCondition> actorConditions,
		std::function<bool(Graph::Node*)> nodeCondition)
	{
		if (!g_originalGetRandomNodeInRange) {
			return nullptr;
		}

		const auto settings = SizeDiff::Config::Get();
		if (settings.mode != SizeDiff::Config::Mode::Strict) {
			return g_originalGetRandomNodeInRange(self, distance, std::move(actorConditions), std::move(nodeCondition));
		}

		const uint32_t contextThread = SizeDiff::Filter::ResolveGraphHookThreadId();
		if (SizeDiff::Filter::ShouldBypassFiltering(contextThread, settings)) {
			return g_originalGetRandomNodeInRange(self, distance, std::move(actorConditions), std::move(nodeCondition));
		}

		const auto scales = SizeDiff::State::GetScales(contextThread);
		if (scales.empty()) {
			return g_originalGetRandomNodeInRange(self, distance, std::move(actorConditions), std::move(nodeCondition));
		}

		const auto cache = SizeDiff::SceneCache::Get();
		const float tolerance = settings.tolerance;

		auto wrapped = [original = std::move(nodeCondition), cache, scales, tolerance, settings](Graph::Node* node) -> bool {
			if (original && !original(node)) {
				return false;
			}
			if (!node) {
				return false;
			}
			const bool match = cache->Matches(node->getNodeID(), scales, tolerance);
			if (match) {
				spdlog::trace("getRandomNodeInRange: Approved {}", node->getNodeID());
				return true;
			}
			if (settings.fallbackBehavior == 1) {
				spdlog::info("getRandomNodeInRange: Soft fallback allowing {} (tolerance={})", node->getNodeID(), tolerance);
				return true;
			}
			if (settings.fallbackBehavior == 2) {
				spdlog::warn("getRandomNodeInRange: Refusing {} (scale mismatch, tolerance={})", node->getNodeID(), tolerance);
			} else {
				spdlog::info("getRandomNodeInRange: Rejected {} due to scale difference (tolerance={})", node->getNodeID(), tolerance);
			}
			return false;
		};

		return g_originalGetRandomNodeInRange(self, distance, std::move(actorConditions), std::move(wrapped));
	}
}

bool SizeDiff::Hooks::InstallNavigationHook()
{
	const auto version = SizeDiff::AddressResolution::GetOStimVersionString();
	if (!version || !SizeDiff::AddressResolution::IsKnownGoodVersion(*version)) {
		return false;
	}

	constexpr auto kSymbol =
		"?getRandomNodeInRange@Node@Graph@@QEAAPEAU12@HV?$vector@UActorCondition@Trait@@V?$allocator@UActorCondition@Trait@@@std@@@std@@V?$function@$$A6A_NPEAUNode@Graph@@@Z@4@@Z";
	auto target = SizeDiff::AddressResolution::ResolveByPdbSymbol(kSymbol);
	if (!target) {
		target = SizeDiff::AddressResolution::ResolveByPattern({
			.version = *version,
			.pattern = "40 55 53 56 57 41 56 41 57 48 8D 6C 24 D1 48 81 EC D8 00 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 17 49 8B F9 4D 8B F0 8B F2 48 8B D9"
		});
	}
	if (!target) {
		spdlog::warn("Could not resolve Graph::Node::getRandomNodeInRange; running without navigation hook");
		return false;
	}

	const auto targetAddr = reinterpret_cast<void*>(*target);
	if (MH_CreateHook(targetAddr, reinterpret_cast<void*>(&HookedGetRandomNodeInRange), reinterpret_cast<void**>(&g_originalGetRandomNodeInRange)) != MH_OK) {
		spdlog::error("Failed to create hook for getRandomNodeInRange");
		return false;
	}
	if (MH_EnableHook(targetAddr) != MH_OK) {
		spdlog::error("Failed to enable hook for getRandomNodeInRange");
		return false;
	}
	spdlog::info("Installed getRandomNodeInRange hook (MinHook) at 0x{:X}", *target);
	return true;
}
