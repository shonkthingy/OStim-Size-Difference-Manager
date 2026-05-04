#pragma once

#include "Plugin.h"
#include "Util/Log.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <format>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>

namespace SizeDiff::Logger
{
	inline void Init()
	{
		using namespace std::literals;
		auto logs = SKSE::log::log_directory();
		if (!logs) {
			SKSE::stl::report_and_fail("Unable to locate SKSE log directory.");
		}

		*logs /= std::format("{}.log", SizeDiff::kPluginName);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logs->string(), true);
		auto logger = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::warn);

		spdlog::set_default_logger(std::move(logger));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
		spdlog::info(
			"[LOGGER_INIT] sink={} level={} flush_on={}",
			logs->string(),
			Log::ToString(spdlog::level::info),
			Log::ToString(spdlog::level::warn));
	}
}
