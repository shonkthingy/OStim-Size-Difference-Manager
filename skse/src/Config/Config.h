#pragma once

#include <atomic>
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
	void Save();
	void Set(Settings settings);
	Settings Get();
	Mode GetMode();
	float GetTolerance();
}
