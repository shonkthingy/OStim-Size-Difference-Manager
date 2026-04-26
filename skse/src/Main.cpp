#include "Plugin.h"

#include "Config/Config.h"
#include "Hooks/GetRandomNodeHook.h"
#include "Hooks/ThreadContextHook.h"
#include "SceneCache/SceneLoader.h"
#include "Util/Logger.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v{};
	v.PluginName(SizeDiff::kPluginName);
	v.PluginVersion(1);
	v.UsesAddressLibrary(true);
	v.HasNoStructUse(true);
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
	return v;
}();

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* message)
	{
		if (!message) {
			return;
		}

		if (message->type == SKSE::MessagingInterface::kPostLoad) {
			// Request OStim's interface map now that all plugins have loaded
			SizeDiff::Hooks::RequestOStimInterface();
		}

		if (message->type == SKSE::MessagingInterface::kDataLoaded) {
			SizeDiff::SceneCache::StartBackgroundScan();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	if (const auto mh = MH_Initialize(); mh != MH_OK) {
		SizeDiff::Logger::Init();
		spdlog::error("MH_Initialize failed: {}", static_cast<int>(mh));
		return false;
	}

	SizeDiff::Logger::Init();
	SKSE::Init(skse);

	spdlog::info("{} v{} loading", SizeDiff::kPluginName, SizeDiff::kPluginVersion);
	spdlog::info("_MSC_FULL_VER={}, _MSVC_STL_VERSION={}", _MSC_FULL_VER, _MSVC_STL_VERSION);

	if (!GetModuleHandleA("OStim.dll")) {
		spdlog::warn("OStim.dll not found. {} will stay idle.", SizeDiff::kPluginName);
		return true;
	}

	SizeDiff::Config::Load();

	auto* messaging = SKSE::GetMessagingInterface();
	if (messaging) {
		messaging->RegisterListener(OnMessage);
	}

	const auto contextOk = SizeDiff::Hooks::InstallThreadContextHooks();
	const auto nodeHookOk = SizeDiff::Hooks::InstallGetRandomNodeHook();
	spdlog::info("Hook setup complete: context={}, getRandomNode={}", contextOk, nodeHookOk);
	return true;
}
