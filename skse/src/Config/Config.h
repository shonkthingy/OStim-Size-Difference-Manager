#pragma once

#include <atomic>
#include <string>

namespace SizeDiff::Config
{
	enum class Mode
	{
		Off = 0,
		Soft = 1,
		Strict = 2
	};

	struct Settings
	{
		Mode mode{ Mode::Strict };
		float tolerance{ 0.10F };
		bool applyToPlayerScenes{ true };
		bool applyToNpcScenes{ true };
		bool applyInAutoMode{ true };
		int fallbackBehavior{ 0 };  // 0 widen, 1 allow any, 2 refuse(log)
	};

	void Load();
	void Reload();
	Settings Get();
	Mode GetMode();
	float GetTolerance();
}
