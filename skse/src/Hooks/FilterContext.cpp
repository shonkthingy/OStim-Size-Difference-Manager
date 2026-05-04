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
		static bool reportedNull = false;
		if (!cached && !reportedNull) {
			spdlog::warn("[FILTER_THREAD_API] unavailable; using state fallback thread resolution");
			reportedNull = true;
		}
		return cached;
	}

	/// OStim's view of which thread the player is in (preferred), else our listener-tracked id.
	uint32_t CanonicalPlayerOStimThreadId()
	{
		if (auto* api = ThreadApi()) {
			const uint32_t fromApi = api->GetPlayerThreadID();
			if (fromApi != 0) {
				return fromApi;
			}
		}
		return SizeDiff::State::GetPlayerThreadId();
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
	// If we only use State::GetPlayerThreadId() for the comparison, isPlayerScene can stay false
	// (listeners not fired yet) while Resolve*ThreadId() already returns GetPlayerThreadID() from
	// the OStim API — then "Filter Player Scenes" / "Filter NPC Scenes" never match reality.
	const uint32_t playerThread = CanonicalPlayerOStimThreadId();
	// OStim may use thread id 0; treat player scenes by id equality (threadId == playerThread).
	const bool isPlayerScene = (threadId == playerThread);

	if (!settings.applyToPlayerScenes && isPlayerScene) {
		spdlog::trace("filter bypassed: player scene filtering disabled (thread={})", threadId);
		return true;
	}
	// "NPC" here means any OStim thread that is not the player's current thread (by id equality).
	if (!settings.applyToNpcScenes && !isPlayerScene) {
		spdlog::trace("filter bypassed: npc scene filtering disabled (thread={})", threadId);
		return true;
	}
	// IsAutoMode(0) is false; if thread is unknown, auto-detect cannot apply this bypass.
	if (!settings.applyInAutoMode && threadId != 0 && QueryIsAutoMode(threadId)) {
		spdlog::trace("filter bypassed: auto mode filtering disabled (thread={})", threadId);
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
