/*
 * OStimNG public thread API (RequestPluginAPI_Thread). Source: OStimNG ModAPI.
 * Trimmed to client-only declarations (no OStim-internal notify symbols).
 */
#pragma once

#include <REL/Version.h>
#include <Windows.h>
#include <cstdint>

namespace OstimNG_API::Thread
{
	using f32 = float;
	static_assert(sizeof(f32) == 4, "OStimNG API: f32 must be 32-bit IEEE 754");

	enum class InterfaceVersion : uint8_t
	{
		V1
	};

	enum class APIResult : uint8_t
	{
		OK,
		Invalid,
		Failed
	};

	enum class ThreadEvent : uint8_t
	{
		ThreadStarted,
		ThreadEnded,
		NodeChanged,
		ControlInput
	};

	enum class Controls : uint8_t
	{
		Up,
		Down,
		Left,
		Right,
		Toggle,
		Yes,
		No,
		Menu,
		KEY_HIDE,
		AlignMenu,
		SearchMenu
	};

	struct KeyData
	{
		int32_t keyUp;
		int32_t keyDown;
		int32_t keyLeft;
		int32_t keyRight;
		int32_t keyYes;
		int32_t keyEnd;
		int32_t keyToggle;
		int32_t keySearch;
		int32_t keyAlignment;
		int32_t keySceneStart;
		int32_t keyNpcSceneStart;
		int32_t keySpeedUp;
		int32_t keySpeedDown;
		int32_t keyPullOut;
		int32_t keyAutoMode;
		int32_t keyFreeCam;
		int32_t keyHideUI;
	};

	typedef void (*ThreadEventCallback)(ThreadEvent eventType, uint32_t threadID, void* userData);
	typedef void (*ControlEventCallback)(Controls controlType, uint32_t threadID, void* userData);

	struct ActorData
	{
		uint32_t formID;
		f32 excitement;
		bool isFemale;
		bool hasSchlong;
		int32_t timesClimaxed;
	};

	struct NavigationData
	{
		const char* sceneId;
		const char* destinationId;
		const char* icon;
		const char* description;
		const char* border;
		bool isTransition;
	};

	struct ActorAlignmentData
	{
		f32 offsetX;
		f32 offsetY;
		f32 offsetZ;
		f32 scale;
		f32 rotation;
		f32 sosBend;
	};

	struct SceneSearchResult
	{
		const char* sceneId;
		const char* name;
		uint32_t actorCount;
	};

	struct OptionsMenuItem
	{
		const char* id;
		const char* title;
		const char* icon;
		const char* border;
		const char* description;
	};

	class IThreadInterface
	{
	public:
		virtual uint32_t GetPlayerThreadID() noexcept = 0;
		virtual bool IsThreadValid(uint32_t threadID) noexcept = 0;
		virtual const char* GetCurrentSceneID(uint32_t threadID) noexcept = 0;
		virtual uint32_t GetActorCount(uint32_t threadID) noexcept = 0;
		virtual uint32_t GetActors(uint32_t threadID, ActorData* buffer, uint32_t bufferSize) noexcept = 0;
		virtual uint32_t GetNavigationCount(uint32_t threadID) noexcept = 0;
		virtual uint32_t GetNavigationOptions(uint32_t threadID, NavigationData* buffer, uint32_t bufferSize) noexcept = 0;
		virtual APIResult NavigateToScene(uint32_t threadID, const char* sceneID) noexcept = 0;
		virtual bool IsTransition(uint32_t threadID) noexcept = 0;
		virtual bool IsInSequence(uint32_t threadID) noexcept = 0;
		virtual bool IsAutoMode(uint32_t threadID) noexcept = 0;
		virtual bool IsPlayerControlDisabled(uint32_t threadID) noexcept = 0;
		virtual void RegisterEventCallback(ThreadEventCallback callback, void* userData) noexcept = 0;
		virtual void UnregisterEventCallback(ThreadEventCallback callback) noexcept = 0;
		virtual void RegisterControlCallback(ControlEventCallback callback, void* userData) noexcept = 0;
		virtual void UnregisterControlCallback(ControlEventCallback callback) noexcept = 0;
		virtual void SetExternalUIEnabled(bool enabled) noexcept = 0;
		virtual void GetKeyData(KeyData* outData) noexcept = 0;
		virtual const char* GetCurrentNodeName(uint32_t threadID) noexcept = 0;
		virtual int32_t GetCurrentSpeed(uint32_t threadID) noexcept = 0;
		virtual int32_t GetMaxSpeed(uint32_t threadID) noexcept = 0;
		virtual APIResult SetSpeed(uint32_t threadID, int32_t speed) noexcept = 0;
		virtual bool GetActorAlignment(uint32_t threadID, uint32_t actorIndex, ActorAlignmentData* outData) noexcept = 0;
		virtual APIResult SetActorAlignment(uint32_t threadID, uint32_t actorIndex, const ActorAlignmentData* data) noexcept = 0;
		virtual uint32_t SearchScenes(const char* query, SceneSearchResult* buffer, uint32_t bufferSize) noexcept = 0;
		virtual bool GetSceneInfo(const char* sceneID, SceneSearchResult* outInfo) noexcept = 0;
		virtual APIResult NavigateToSearchResult(uint32_t threadID, const char* sceneID) noexcept = 0;
		virtual void RebuildOptionsTree() noexcept = 0;
		virtual uint32_t GetOptionsItemCount() noexcept = 0;
		virtual uint32_t GetOptionsItems(OptionsMenuItem* buffer, uint32_t bufferSize) noexcept = 0;
		virtual bool SelectOptionsItem(int32_t index) noexcept = 0;
		virtual bool IsOptionsAtRoot() noexcept = 0;
		virtual bool IsActorInAnyThread(uint32_t actorFormID) noexcept = 0;
		virtual bool HasCompatibleNode(uint32_t threadID, const uint32_t* actorFormIDs, uint32_t actorCount) noexcept = 0;
		virtual int32_t MigrateThread(uint32_t threadID, const uint32_t* actorFormIDs, uint32_t actorCount,
			void (*onComplete)(int32_t newThreadID, void* context) = nullptr,
			void* context = nullptr,
			int startDelayMs = 500) noexcept = 0;
		virtual bool IsUnrestrictedNavigation() noexcept = 0;
		virtual bool IsIntendedSexOnly() noexcept = 0;
		virtual int32_t GetActorPosition(uint32_t threadID, uint32_t actorFormID) noexcept = 0;
	};

	using _RequestPluginAPI_Thread = IThreadInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion);

	inline IThreadInterface* GetAPI(const char* pluginName, REL::Version pluginVersion)
	{
		const auto ostim = GetModuleHandleA("OStim.dll");
		if (!ostim) {
			return nullptr;
		}
		const auto requestAPI = reinterpret_cast<_RequestPluginAPI_Thread>(
			reinterpret_cast<void*>(GetProcAddress(ostim, "RequestPluginAPI_Thread")));
		if (!requestAPI) {
			return nullptr;
		}
		return requestAPI(InterfaceVersion::V1, pluginName, pluginVersion);
	}
}
