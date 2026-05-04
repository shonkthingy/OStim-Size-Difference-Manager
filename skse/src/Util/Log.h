#pragma once

#include <chrono>
#include <string_view>

#include <spdlog/spdlog.h>

namespace SizeDiff::Log
{
	enum class ConfigSource
	{
		Startup,
		DiskLoad,
		DiskReload,
		UI,
		Runtime
	};

	std::string_view ToString(ConfigSource source);
	std::string_view ToString(spdlog::level::level_enum level);

	spdlog::level::level_enum ParseLevel(std::string_view rawLevel, spdlog::level::level_enum fallback, bool* coerced = nullptr);
	void ApplyRuntimeLevel(spdlog::level::level_enum level, ConfigSource source);

	// Shared helper for repetitive logs from hot paths.
	bool ShouldLogNow(std::string_view key, std::chrono::milliseconds interval);
}
