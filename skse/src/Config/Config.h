#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace SizeDiff::Config
{
	enum class Mode
	{
		Off = 0,
		Strict = 1,
		Soft = 2,
		Debug = 3
	};

	struct Settings
	{
		Mode mode{ Mode::Strict };
		float tolerance{ 0.10F };
		bool applyToPlayerScenes{ true };
		bool applyToNpcScenes{ true };
		bool applyInAutoMode{ true };
	};

	void Load();
	void Reload();
	void Set(Settings settings);
	Settings Get();
	Mode GetMode();
	float GetTolerance();

	enum class PersistStatus : std::uint8_t
	{
		Saved = 0,
		Dirty = 1,
		SaveFailed = 2
	};

	bool TryAutosave(std::chrono::steady_clock::time_point now, std::chrono::milliseconds debounce);
	bool FlushDirtyNow();
	PersistStatus GetPersistStatus();
}
