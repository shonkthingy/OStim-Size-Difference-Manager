#include "Hooks/MenuFilterHook.h"

#include "AddressResolution/PatternScanner.h"
#include "AddressResolution/PdbResolver.h"
#include "AddressResolution/VersionGate.h"
#include "Config/Config.h"
#include "Hooks/FilterContext.h"
#include "OStimTypes/ActorCondition.h"
#include "OStimTypes/Navigation.h"
#include "SceneCache/SceneCache.h"
#include "Util/State.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

namespace
{
	// x64: RCX = this, RDX.. = std::vector<ActorCondition> by value
	using FulfilledBy_t = bool (*)(
		Graph::Navigation* self,
		std::vector<Trait::ActorCondition> conditions);

	FulfilledBy_t g_originalFulfilledBy{ nullptr };

	bool HookedFulfilledBy(
		Graph::Navigation* self,
		std::vector<Trait::ActorCondition> conditions)
	{
		if (!g_originalFulfilledBy) {
			return false;
		}
		const bool orig = g_originalFulfilledBy(self, std::move(conditions));
		if (!orig) {
			return false;
		}
		if (!self || self->nodes.empty()) {
			return true;
		}

		Graph::Node* const dest = self->nodes.back();
		if (!dest) {
			return true;
		}

		const auto settings = SizeDiff::Config::Get();
		if (settings.mode != SizeDiff::Config::Mode::Strict) {
			return true;
		}

		const uint32_t contextThread = SizeDiff::Filter::ResolveMenuHookThreadId();
		if (SizeDiff::Filter::ShouldBypassFiltering(contextThread, settings)) {
			return true;
		}

		const auto scales = SizeDiff::State::GetScales(contextThread);
		if (scales.empty()) {
			spdlog::trace("fulfilledBy: Strict mode but no scales for thread {} in State; allowing navigation entry", contextThread);
			return true;
		}

		const float tolerance = settings.tolerance;
		const auto cache = SizeDiff::SceneCache::Get();
		const char* const nodeId = dest->getNodeID();
		if (!nodeId) {
			return true;
		}

		if (cache->Matches(nodeId, scales, tolerance)) {
			spdlog::trace("fulfilledBy: Scene '{}' matches scales", nodeId);
			return true;
		}
		if (settings.fallbackBehavior == 1) {
			spdlog::info("fulfilledBy: Soft fallback showing '{}' (tolerance={})", nodeId, tolerance);
			return true;
		}
		if (settings.fallbackBehavior == 2) {
			spdlog::warn("fulfilledBy: Hiding menu entry for '{}' (scale mismatch, tolerance={})", nodeId, tolerance);
		} else {
			spdlog::info("fulfilledBy: Hiding menu entry for '{}' (scale mismatch, tolerance={})", nodeId, tolerance);
		}
		return false;
	}
}

bool SizeDiff::Hooks::InstallMenuFilterHook()
{
	const auto version = SizeDiff::AddressResolution::GetOStimVersionString();
	if (!version || !SizeDiff::AddressResolution::IsKnownGoodVersion(*version)) {
		return false;
	}

	// See OStim Graph::Node.cpp — explicit vector pass by value. PDB often exposes undecorated name.
	constexpr auto kMangled =
		"?fulfilledBy@Navigation@Graph@@QEAA_NV?$vector@UActorCondition@Trait@@V?$allocator@UActorCondition@Trait@@@std@@@std@@@Z";

	auto target = SizeDiff::AddressResolution::ResolveByPdbSymbol(kMangled);
	if (!target) {
		target = SizeDiff::AddressResolution::ResolveByPdbSymbol("Graph::Navigation::fulfilledBy");
	}
	if (!target) {
		target = SizeDiff::AddressResolution::ResolveByPattern({
			.version = *version,
			// Dumped from OStim 7.4.x (RVA 0x1757E0)
			.pattern = "48 89 5C 24 18 48 89 54 24 10 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 40 48 8B F2 48 8B 39 48 8B 69 08 48 3B FD"
		});
	}
	if (!target) {
		spdlog::warn("Could not resolve Graph::Navigation::fulfilledBy; menu filtering disabled");
		return false;
	}

	const auto targetAddr = reinterpret_cast<void*>(*target);
	if (MH_CreateHook(targetAddr, reinterpret_cast<void*>(&HookedFulfilledBy), reinterpret_cast<void**>(&g_originalFulfilledBy)) != MH_OK) {
		spdlog::error("Failed to create hook for Navigation::fulfilledBy");
		return false;
	}
	if (MH_EnableHook(targetAddr) != MH_OK) {
		spdlog::error("Failed to enable hook for Navigation::fulfilledBy");
		return false;
	}
	spdlog::info("Installed Graph::Navigation::fulfilledBy hook (MinHook) at 0x{:X}", *target);
	return true;
}
