#include "Hooks/FilterContext.h"

#include <string_view>

#include "OStimAPI/OstimNGThreadAPI.h"
#include "Plugin.h"
#include "Util/State.h"

namespace
{
	OstimNG_API::Thread::IThreadInterface* ThreadApi()
	{
		static OstimNG_API::Thread::IThreadInterface* cached = OstimNG_API::Thread::GetAPI(
			SizeDiff::kPluginName,
			REL::Version(std::string_view(SizeDiff::kPluginVersion)));
		return cached;
	}
}

uint32_t SizeDiff::Filter::ResolveGraphHookThreadId()
{
	const uint32_t last = State::GetLastActiveThreadId();
	if (last != 0) {
		return last;
	}
	if (auto* api = ThreadApi()) {
		return api->GetPlayerThreadID();
	}
	return State::GetPlayerThreadId();
}

uint32_t SizeDiff::Filter::ResolveMenuHookThreadId()
{
	if (auto* api = ThreadApi()) {
		const uint32_t id = api->GetPlayerThreadID();
		if (id != 0) {
			return id;
		}
	}
	return State::GetPlayerThreadId();
}

bool SizeDiff::Filter::ShouldBypassFiltering(uint32_t threadId, const Config::Settings& settings)
{
	if (threadId == 0) {
		return false;
	}

	const uint32_t playerId = State::GetPlayerThreadId();
	const bool isPlayerScene = (playerId != 0 && threadId == playerId);

	if (!settings.applyToPlayerScenes && isPlayerScene) {
		return true;
	}
	if (!settings.applyToNpcScenes && !isPlayerScene) {
		return true;
	}
	if (!settings.applyInAutoMode && QueryIsAutoMode(threadId)) {
		return true;
	}
	return false;
}

bool SizeDiff::Filter::QueryIsAutoMode(uint32_t threadId)
{
	if (threadId == 0) {
		return false;
	}
	if (auto* api = ThreadApi()) {
		return api->IsAutoMode(threadId);
	}
	return false;
}
