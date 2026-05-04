#pragma once

#include <string>

#include "Util/Log.h"

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
		spdlog::level::level_enum logLevel{ spdlog::level::info };
	};

	void Load(Log::ConfigSource source = Log::ConfigSource::DiskLoad);
	void Reload();
	void Save(Log::ConfigSource source = Log::ConfigSource::UI);
	void Set(Settings settings);
	void SetFromSource(Settings settings, Log::ConfigSource source);
	Settings Get();
	Mode GetMode();
	float GetTolerance();
}
