#include "Hooks/GetRandomNodeHook.h"

#include "AddressResolution/PdbResolver.h"
#include "AddressResolution/PatternScanner.h"
#include "AddressResolution/VersionGate.h"
#include "Config/Config.h"
#include "OStimTypes/ActorCondition.h"
#include "OStimTypes/FurnitureType.h"
#include "OStimTypes/Node.h"
#include "SceneCache/SceneCache.h"
#include "Util/Tls.h"

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

		const auto mode = SizeDiff::Config::GetMode();
		const auto scales = SizeDiff::Tls::CurrentScales();
		if (mode == SizeDiff::Config::Mode::Strict && !scales.empty()) {
			const auto cache = SizeDiff::SceneCache::Get();
			const float tolerance = SizeDiff::Config::GetTolerance();

			auto wrapped = [original = std::move(nodeCondition), cache, scales, tolerance](Graph::Node* node) -> bool {
				if (original && !original(node)) {
					return false;
				}
				if (!node) {
					return false;
				}
				return cache->Matches(node->getNodeID(), scales, tolerance);
			};

			return g_originalGetRandomNode(furnitureType, std::move(actorConditions), std::move(wrapped));
		}

		return g_originalGetRandomNode(furnitureType, std::move(actorConditions), std::move(nodeCondition));
	}
}

bool SizeDiff::Hooks::InstallGetRandomNodeHook()
{
	const auto version = SizeDiff::AddressResolution::GetOStimVersionString();
	if (!version || !SizeDiff::AddressResolution::IsKnownGoodVersion(*version)) {
		return false;
	}

	constexpr auto kSymbol = "?getRandomNode@GraphTable@Graph@@SAPEAUNode@2@PEAVFurnitureType@Furniture@2@V?$vector@UActorCondition@Trait@@V?$allocator@UActorCondition@Trait@@@std@@@std@@V?$function@$$A6A_NPEAUNode@Graph@@@Z@std@@@Z";
	auto target = SizeDiff::AddressResolution::ResolveByPdbSymbol(kSymbol);
	if (!target) {
		target = SizeDiff::AddressResolution::ResolveByPattern({
			.version = *version,
			.pattern = "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 00 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 78 49 8B F0 4C 8B FA 4C 8B E1"
		});
	}
	if (!target) {
		spdlog::warn("Could not resolve Graph::GraphTable::getRandomNode; running without hook");
		return false;
	}

	SKSE::AllocTrampoline(128);
	auto& trampoline = SKSE::GetTrampoline();
	g_originalGetRandomNode = reinterpret_cast<GetRandomNode_t>(trampoline.write_call<5>(*target, reinterpret_cast<uintptr_t>(HookedGetRandomNode)));
	spdlog::info("Installed getRandomNode hook at 0x{:X}", *target);
	return true;
}
