#include "Hooks/GetRandomNodeHook.h"

#include "AddressResolution/PdbResolver.h"
#include "AddressResolution/PatternScanner.h"
#include "AddressResolution/VersionGate.h"
#include "Config/Config.h"
#include "Hooks/FilterContext.h"
#include "Hooks/RandomNodeTwoPass.h"
#include "OStimTypes/ActorCondition.h"
#include "OStimTypes/FurnitureType.h"
#include "OStimTypes/Node.h"
#include "SceneCache/SceneCache.h"
#include "Util/State.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

namespace
{
	using GetRandomNode_t = Graph::Node* (*)(
		Furniture::FurnitureType*,
		std::vector<Trait::ActorCondition>,
		std::function<bool(Graph::Node*)>);

	GetRandomNode_t g_originalGetRandomNode{ nullptr };

	Graph::Node* HookedGetRandomNode(
		Furniture::FurnitureType* furnitureType,
		std::vector<Trait::ActorCondition> actorConditions,
		std::function<bool(Graph::Node*)> nodeCondition)
	{
		if (!g_originalGetRandomNode) {
			return nullptr;
		}

		const auto settings = SizeDiff::Config::Get();
		if (settings.mode == SizeDiff::Config::Mode::Off) {
			return g_originalGetRandomNode(furnitureType, std::move(actorConditions), std::move(nodeCondition));
		}

		const uint32_t contextThread = SizeDiff::Filter::ResolveGraphHookThreadId();
		if (SizeDiff::Filter::ShouldBypassFiltering(contextThread, settings)) {
			return g_originalGetRandomNode(furnitureType, std::move(actorConditions), std::move(nodeCondition));
		}

		const auto scales = SizeDiff::State::GetScales(contextThread);
		if (scales.empty()) {
			spdlog::trace("getRandomNode: no scales for thread {}; bypassing size filter", contextThread);
			return g_originalGetRandomNode(furnitureType, std::move(actorConditions), std::move(nodeCondition));
		}

		const auto cache = SizeDiff::SceneCache::Get();

		return SizeDiff::Hooks::RandomNodeTwoPass::Run(
			[furnitureType](std::vector<Trait::ActorCondition> ac, std::function<bool(Graph::Node*)> pred) -> Graph::Node* {
				return g_originalGetRandomNode(furnitureType, std::move(ac), std::move(pred));
			},
			std::move(actorConditions),
			std::move(nodeCondition),
			scales,
			cache,
			"getRandomNode");
	}
}

bool SizeDiff::Hooks::InstallGetRandomNodeHook()
{
	const auto version = SizeDiff::AddressResolution::GetOStimVersionString();
	if (!version) {
		spdlog::warn("[HOOK_RESOLVE_FAIL] hook=getRandomNode reason=missing_ostim_version");
		return false;
	}
	if (!SizeDiff::AddressResolution::IsKnownGoodVersion(*version)) {
		return false;
	}

	constexpr auto kSymbol = "?getRandomNode@GraphTable@Graph@@SAPEAUNode@2@PEAVFurnitureType@Furniture@2@V?$vector@UActorCondition@Trait@@V?$allocator@UActorCondition@Trait@@@std@@@std@@V?$function@$$A6A_NPEAUNode@Graph@@@Z@std@@@Z";
	auto target = SizeDiff::AddressResolution::ResolveByPdbSymbol(kSymbol);
	const char* strategy = "pdb_mangled";
	if (!target) {
		const char* const pat = SizeDiff::AddressResolution::UsesLegacyGraphBytePatterns(*version)
			? "4C 89 44 24 18 48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48 83 EC 50 49 8B E8 4C 8B FA 4C 8B E1 E8 ? ? ? ?"
			: "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 00 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 78 49 8B F0 4C 8B FA 4C 8B E1";
		target = SizeDiff::AddressResolution::ResolveByPattern({
			.version = *version,
			.pattern = pat,
		});
		strategy = "pattern";
	}
	if (!target) {
		spdlog::warn("Could not resolve Graph::GraphTable::getRandomNode; running without hook");
		return false;
	}
	spdlog::debug("[HOOK_RESOLVE] hook=getRandomNode strategy={} address=0x{:X}", strategy, *target);

	const auto targetAddr = reinterpret_cast<void*>(*target);
	if (MH_CreateHook(targetAddr, reinterpret_cast<void*>(&HookedGetRandomNode), reinterpret_cast<void**>(&g_originalGetRandomNode)) != MH_OK) {
		spdlog::error("Failed to create hook for getRandomNode");
		return false;
	}
	if (MH_EnableHook(targetAddr) != MH_OK) {
		spdlog::error("Failed to enable hook for getRandomNode");
		return false;
	}
	spdlog::info("Installed getRandomNode hook (MinHook) at 0x{:X}", *target);
	return true;
}
