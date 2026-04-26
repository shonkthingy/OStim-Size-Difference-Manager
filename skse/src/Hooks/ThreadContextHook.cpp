#include "Hooks/ThreadContextHook.h"

#include "PCH.h"
#include "OStimAPI/OStimInterface.h"
#include "Util/Tls.h"

#include "spdlog/spdlog.h"
#include <sstream>

namespace
{
    // Collect scales from an OStim::Thread via its public ABI
    std::vector<float> ScalesFromThread(OStim::Thread* thread)
    {
        if (!thread) return {};
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
            scales.push_back(actor->GetReferenceRuntimeData().refScale / 100.0f);
        }
        return scales;
    }

    // ---------------------------------------------------------------
    // Thread-start listener — fires when OStim begins any new scene
    // ---------------------------------------------------------------
    class SizeMatchThreadListener : public OStim::ThreadEventListener
    {
    public:
        void threadRegistered(OStim::Thread* thread) override
        {
            if (!thread) return;
            auto scales = ScalesFromThread(thread);
            if (scales.empty()) return;

            std::ostringstream scaleStream;
            for (std::size_t i = 0; i < scales.size(); ++i) {
                if (i > 0) {
                    scaleStream << ' ';
                }
                scaleStream << scales[i];
            }

            spdlog::info("OStim thread {} started with {} actors, scales: {}",
                thread->getThreadID(), scales.size(), scaleStream.str());

            // Push onto TLS so the getRandomNode hook can read it.
            // getRandomNode is called synchronously during thread start, so
            // the stack entry will be live for the duration of initial scene selection.
            SizeDiff::Tls::PushScales(scales);
        }
    };

    // ---------------------------------------------------------------
    // Node-change listener — fires on every auto-mode / player navigation.
    // We refresh TLS so subsequent getRandomNode calls see current scales.
    // ---------------------------------------------------------------
    class SizeMatchNodeListener : public OStim::ThreadEventListener
    {
    public:
        void threadRegistered(OStim::Thread* thread) override
        {
            if (!thread) return;
            auto scales = ScalesFromThread(thread);

            // Replace the top of the TLS stack with fresh scales for this thread.
            // (Scales don't change mid-scene but this keeps the stack consistent.)
            if (!SizeDiff::Tls::g_scaleStack.empty()) {
                SizeDiff::Tls::g_scaleStack.back() = std::move(scales);
            } else {
                SizeDiff::Tls::PushScales(std::move(scales));
            }
        }
    };

    // ---------------------------------------------------------------
    // Thread-stop listener — pops TLS when the scene ends
    // ---------------------------------------------------------------
    class SizeMatchStopListener : public OStim::ThreadEventListener
    {
    public:
        void threadRegistered(OStim::Thread* thread) override
        {
            SizeDiff::Tls::PopScales();
        }
    };

    SizeMatchThreadListener g_startListener;
    SizeMatchNodeListener   g_nodeListener;
    SizeMatchStopListener   g_stopListener;

    OStim::ThreadInterface* g_threadInterface = nullptr;
}

bool SizeDiff::Hooks::InstallThreadContextHooks()
{
    // Registration happens in two stages:
    // 1. Here we store a flag that we want to register.
    // 2. In OnOStimMessage() (called from Main.cpp on kPostLoad) we acquire the
    //    ThreadInterface and register the listeners.
    // This function itself just validates the setup compiles.
    spdlog::info("Thread context listener setup ready (pending OStim interface exchange)");
    return true;
}

void SizeDiff::Hooks::RequestOStimInterface()
{
    // OStim's interface exchange is a REQUEST pattern:
    // We SEND a message of type 'OST' to OStim, it fills in the interfaceMap pointer.
    // This must happen on kPostLoad (after OStim has registered its listener).
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
