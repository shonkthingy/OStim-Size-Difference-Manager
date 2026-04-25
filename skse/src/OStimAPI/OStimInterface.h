#pragma once

// Minimal copies of OStim's public plugin interface ABI.
// Based on OStim PluginInterface/*.h — keep in sync with targeted OStim version.

namespace OStim {

    class PluginInterface {
    public:
        PluginInterface() {}
        virtual ~PluginInterface() {}
        virtual uint32_t getVersion() = 0;
    };

    class ThreadActor {
    public:
        virtual void* getGameActor() = 0;  // safe to cast to RE::Actor*
    };

    class Thread {
    public:
        virtual int32_t      getThreadID()    = 0;
        virtual bool         isPlayerThread() = 0;
        virtual uint32_t     getActorCount()  = 0;
        virtual ThreadActor* getActor(uint32_t position) = 0;
        virtual void         forEachThreadActor(void* visitor) = 0;
        virtual void*        getCurrentNode() = 0;
    };

    class ThreadEventListener {
    public:
        virtual void threadRegistered(Thread* thread) = 0;
    };

    class ThreadInterface : public PluginInterface {
    public:
        inline static constexpr const char* NAME = "Threads";
        virtual Thread* getThread(int32_t threadID) = 0;
        virtual void registerThreadStartListener(ThreadEventListener* listener) = 0;
        virtual void registerSpeedChangedListener(ThreadEventListener* listener) = 0;
        virtual void registerNodeChangedListener(ThreadEventListener* listener) = 0;
        virtual void registerClimaxListener(void* listener) = 0;
        virtual void registerThreadStopListener(ThreadEventListener* listener) = 0;
        virtual void* createThreadBuilder(uint32_t actorCount, void** actors) = 0;
    };

    class InterfaceMap {
    public:
        virtual PluginInterface* queryInterface(const char* name) = 0;
        virtual bool addInterface(const char* name, PluginInterface* pluginInterface) = 0;
        virtual PluginInterface* removeInterface(const char* name) = 0;
    };

    struct InterfaceExchangeMessage {
        enum : uint32_t { MESSAGE_TYPE = 'OST' };
        InterfaceMap* interfaceMap = nullptr;
    };

}  // namespace OStim
