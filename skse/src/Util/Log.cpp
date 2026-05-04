#include "Util/Log.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
	using Clock = std::chrono::steady_clock;

	std::mutex g_rateLimitMutex;
	std::unordered_map<std::string, Clock::time_point> g_lastLogByKey;
}

std::string_view SizeDiff::Log::ToString(const ConfigSource source)
{
	switch (source) {
	case ConfigSource::Startup:
		return "startup";
	case ConfigSource::DiskLoad:
		return "disk_load";
	case ConfigSource::DiskReload:
		return "disk_reload";
	case ConfigSource::UI:
		return "ui";
	case ConfigSource::Runtime:
	default:
		return "runtime";
	}
}

std::string_view SizeDiff::Log::ToString(const spdlog::level::level_enum level)
{
	const auto view = spdlog::level::to_string_view(level);
	return std::string_view(view.data(), view.size());
}

spdlog::level::level_enum SizeDiff::Log::ParseLevel(
	std::string_view rawLevel,
	const spdlog::level::level_enum fallback,
	bool* coerced)
{
	std::string lowered(rawLevel);
	std::ranges::transform(lowered, lowered.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	spdlog::level::level_enum out = fallback;
	bool wasCoerced = false;
	if (lowered == "trace") {
		out = spdlog::level::trace;
	} else if (lowered == "debug") {
		out = spdlog::level::debug;
	} else if (lowered == "info") {
		out = spdlog::level::info;
	} else if (lowered == "warn" || lowered == "warning") {
		out = spdlog::level::warn;
	} else if (lowered == "error") {
		out = spdlog::level::err;
	} else if (lowered == "critical") {
		out = spdlog::level::critical;
	} else if (lowered == "off") {
		out = spdlog::level::off;
	} else {
		wasCoerced = true;
	}

	if (coerced) {
		*coerced = wasCoerced;
	}
	return out;
}

void SizeDiff::Log::ApplyRuntimeLevel(const spdlog::level::level_enum level, const ConfigSource source)
{
	auto logger = spdlog::default_logger();
	if (!logger) {
		return;
	}

	logger->set_level(level);
	logger->flush_on(spdlog::level::warn);
	spdlog::info(
		"[LOG_LEVEL_APPLIED] source={} level={} flush_on={}",
		ToString(source),
		ToString(level),
		ToString(spdlog::level::warn));
}

bool SizeDiff::Log::ShouldLogNow(const std::string_view key, const std::chrono::milliseconds interval)
{
	const auto now = Clock::now();
	std::scoped_lock lock(g_rateLimitMutex);
	const auto it = g_lastLogByKey.find(std::string(key));
	if (it != g_lastLogByKey.end() && now - it->second < interval) {
		return false;
	}
	g_lastLogByKey[std::string(key)] = now;
	return true;
}
