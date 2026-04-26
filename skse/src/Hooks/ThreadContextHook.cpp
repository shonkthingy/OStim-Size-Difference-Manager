#include "Hooks/ThreadContextHook.h"

#include "PCH.h"
#include "OStimAPI/OStimInterface.h"
#include "Util/State.h"

#include "spdlog/spdlog.h"
#include <sstream>

namespace
{
	// Collect scales from an OStim::Thread via its public ABI
	std::vector<float> ScalesFromThread(OStim::Thread* thread)
	{
		if (!thread) {
			return {};
		}
		std::vector<float> scales;
		const uint32_t count = thread->getActorCount();
		scales.reserve(count);
		for (uint32_t i = 0; i < count; ++i) {
			OStim::ThreadActor* ta = thread->getActor(i);
			if (!ta) {
				scales.push_back(1.0f);
				continue;
			}
			auto* actor = static_cast<RE::Actor*>(ta->getGameActor());
			if (!actor) {
				scales.push_back(1.0f);
				continue;
			}

			const auto& actorData = actor->GetActorRuntimeData();
			const auto& refData = actor->GetReferenceRuntimeData();
			const RE::TESNPC* actorBase = actor->GetActorBase();
			const uint8_t gender = (actorBase && actorBase->GetSex() == RE::SEXES::SEX::kFemale) ? 1u : 0u;

			const float refScale = static_cast<float>(refData.refScale) / 100.0f;
			float raceHeight = 1.0f;

			if (const RE::TESRace* race = actorData.race; race) {
				raceHeight = race->data.height[gender];
			}

			const float finalScale = raceHeight * refScale;
			scales.push_back(finalScale);

			const RE::TESRace* race = actorData.race;
			const char* raceName = "<null>";
			if (race) {
				if (const char* name = race->GetName(); name && name[0] != '\0') {
					raceName = name;
				} else {
					raceName = "<unnamed>";
				}
			}

			spdlog::info(
				"ScalesFromThread actor debug: race='{}' gender={} raceHeight={} refScale={} finalScale={}",
				raceName,
				gender,
				raceHeight,
				refScale,
				finalScale);
		}
		return scales;
	}

	// ---------------------------------------------------------------
	// Thread-start — cache scales and track player OStim thread id
	// ---------------------------------------------------------------
	class SizeMatchThreadListener : public OStim::ThreadEventListener
	{
	public:
		void threadRegistered(OStim::Thread* thread) override
		{
			if (!thread) {
				return;
			}
			auto scales = ScalesFromThread(thread);
			if (scales.empty()) {
				return;
			}

			const uint32_t tid = static_cast<uint32_t>(thread->getThreadID());

			std::ostringstream scaleStream;
			for (std::size_t i = 0; i < scales.size(); ++i) {
				if (i > 0) {
					scaleStream << ' ';
				}
				scaleStream << scales[i];
			}

			spdlog::info("OStim thread {} started with {} actors, scales: {}",
				tid, scales.size(), scaleStream.str());

			SizeDiff::State::SetScales(tid, std::move(scales));
			if (thread->isPlayerThread()) {
				SizeDiff::State::SetPlayerThreadId(tid);
			}
		}
	};

	// ---------------------------------------------------------------
	// Node change — refresh scales for this OStim thread
	// ---------------------------------------------------------------
	class SizeMatchNodeListener : public OStim::ThreadEventListener
	{
	public:
		void threadRegistered(OStim::Thread* thread) override
		{
			if (!thread) {
				return;
			}
			auto scales = ScalesFromThread(thread);
			const uint32_t tid = static_cast<uint32_t>(thread->getThreadID());
			SizeDiff::State::SetScales(tid, std::move(scales));
			if (thread->isPlayerThread()) {
				SizeDiff::State::SetPlayerThreadId(tid);
			}
		}
	};

	// ---------------------------------------------------------------
	// Thread stop — remove scales; clear player id if this was the player thread
	// ---------------------------------------------------------------
	class SizeMatchStopListener : public OStim::ThreadEventListener
	{
	public:
		void threadRegistered(OStim::Thread* thread) override
		{
			if (!thread) {
				return;
			}
			const uint32_t tid = static_cast<uint32_t>(thread->getThreadID());
			SizeDiff::State::ClearScales(tid);
			if (thread->isPlayerThread() && SizeDiff::State::GetPlayerThreadId() == tid) {
				SizeDiff::State::SetPlayerThreadId(0);
			}
		}
	};

	SizeMatchThreadListener g_startListener;
	SizeMatchNodeListener   g_nodeListener;
	SizeMatchStopListener   g_stopListener;

	OStim::ThreadInterface* g_threadInterface = nullptr;
}

bool SizeDiff::Hooks::InstallThreadContextHooks()
{
	spdlog::info("Thread context listener setup ready (pending OStim interface exchange)");
	return true;
}

void SizeDiff::Hooks::RequestOStimInterface()
{
	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		spdlog::warn("Could not get SKSE messaging interface");
		return;
	}

	OStim::InterfaceExchangeMessage exchange;
	messaging->Dispatch(
		OStim::InterfaceExchangeMessage::MESSAGE_TYPE,
		&exchange,
		sizeof(OStim::InterfaceExchangeMessage*),
		"OStim");

	if (!exchange.interfaceMap) {
		spdlog::warn("OStim did not fill interfaceMap — is OStim loaded and version compatible?");
		return;
	}

	auto* iface = exchange.interfaceMap->queryInterface(OStim::ThreadInterface::NAME);
	if (!iface) {
		spdlog::warn("OStim ThreadInterface not found in interface map");
		return;
	}

	g_threadInterface = static_cast<OStim::ThreadInterface*>(iface);
	g_threadInterface->registerThreadStartListener(&g_startListener);
	g_threadInterface->registerNodeChangedListener(&g_nodeListener);
	g_threadInterface->registerThreadStopListener(&g_stopListener);

	spdlog::info("Registered OStim thread lifecycle listeners via public API");
}
