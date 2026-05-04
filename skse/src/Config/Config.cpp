#include "Config/Config.h"

#include "Plugin.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <vector>

namespace
{
	std::mutex g_mutex;
	SizeDiff::Config::Settings g_settings{};
	inline constexpr auto kIniPath = L"Data/SKSE/Plugins/OStimSizeDifferenceManager.ini";
	inline constexpr auto kIniPathLog = "Data/SKSE/Plugins/OStimSizeDifferenceManager.ini";

	bool ParseBool(const std::string& value)
	{
		return value == "1" || value == "true" || value == "True" || value == "TRUE";
	}

	bool NearlyEqual(const float a, const float b)
	{
		return std::fabs(a - b) <= 1e-6F;
	}

	bool SettingsEqual(const SizeDiff::Config::Settings& lhs, const SizeDiff::Config::Settings& rhs)
	{
		return lhs.mode == rhs.mode &&
			NearlyEqual(lhs.tolerance, rhs.tolerance) &&
			lhs.applyToPlayerScenes == rhs.applyToPlayerScenes &&
			lhs.applyToNpcScenes == rhs.applyToNpcScenes &&
			lhs.applyInAutoMode == rhs.applyInAutoMode &&
			lhs.logLevel == rhs.logLevel;
	}

	std::string BuildChangedKeys(const SizeDiff::Config::Settings& oldValue, const SizeDiff::Config::Settings& newValue)
	{
		std::vector<std::string> keys;
		if (oldValue.mode != newValue.mode) {
			keys.emplace_back("Mode");
		}
		if (!NearlyEqual(oldValue.tolerance, newValue.tolerance)) {
			keys.emplace_back("Tolerance");
		}
		if (oldValue.applyToPlayerScenes != newValue.applyToPlayerScenes) {
			keys.emplace_back("ApplyToPlayerScenes");
		}
		if (oldValue.applyToNpcScenes != newValue.applyToNpcScenes) {
			keys.emplace_back("ApplyToNpcScenes");
		}
		if (oldValue.applyInAutoMode != newValue.applyInAutoMode) {
			keys.emplace_back("ApplyInAutoMode");
		}
		if (oldValue.logLevel != newValue.logLevel) {
			keys.emplace_back("LogLevel");
		}

		std::ostringstream out;
		for (std::size_t i = 0; i < keys.size(); ++i) {
			if (i > 0) {
				out << ',';
			}
			out << keys[i];
		}
		return out.str();
	}

	void LogConfigDiffs(
		const SizeDiff::Config::Settings& oldValue,
		const SizeDiff::Config::Settings& newValue,
		const SizeDiff::Log::ConfigSource source)
	{
		if (oldValue.mode != newValue.mode) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=Mode oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				static_cast<int>(oldValue.mode),
				static_cast<int>(newValue.mode));
		}
		if (!NearlyEqual(oldValue.tolerance, newValue.tolerance)) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=Tolerance oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				oldValue.tolerance,
				newValue.tolerance);
		}
		if (oldValue.applyToPlayerScenes != newValue.applyToPlayerScenes) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=ApplyToPlayerScenes oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				oldValue.applyToPlayerScenes,
				newValue.applyToPlayerScenes);
		}
		if (oldValue.applyToNpcScenes != newValue.applyToNpcScenes) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=ApplyToNpcScenes oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				oldValue.applyToNpcScenes,
				newValue.applyToNpcScenes);
		}
		if (oldValue.applyInAutoMode != newValue.applyInAutoMode) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=ApplyInAutoMode oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				oldValue.applyInAutoMode,
				newValue.applyInAutoMode);
		}
		if (oldValue.logLevel != newValue.logLevel) {
			spdlog::debug(
				"[CONFIG_FIELD_DIFF] source={} key=LogLevel oldValue={} newValue={} coerced=false",
				SizeDiff::Log::ToString(source),
				SizeDiff::Log::ToString(oldValue.logLevel),
				SizeDiff::Log::ToString(newValue.logLevel));
		}
	}

	void ApplySettings(const SizeDiff::Config::Settings& next, const std::optional<SizeDiff::Log::ConfigSource> source)
	{
		SizeDiff::Config::Settings previous{};
		{
			std::scoped_lock lock(g_mutex);
			previous = g_settings;
			g_settings = next;
		}

		if (previous.logLevel != next.logLevel) {
			SizeDiff::Log::ApplyRuntimeLevel(next.logLevel, source.value_or(SizeDiff::Log::ConfigSource::Runtime));
		}

		if (!source || SettingsEqual(previous, next)) {
			return;
		}

		const auto changedKeys = BuildChangedKeys(previous, next);
		spdlog::info(
			"[CONFIG_CHANGED] source={} changedKeys={} mode={} tolerance={} applyToPlayerScenes={} applyToNpcScenes={} applyInAutoMode={} logLevel={}",
			SizeDiff::Log::ToString(*source),
			changedKeys,
			static_cast<int>(next.mode),
			next.tolerance,
			next.applyToPlayerScenes,
			next.applyToNpcScenes,
			next.applyInAutoMode,
			SizeDiff::Log::ToString(next.logLevel));
		LogConfigDiffs(previous, next, *source);
	}
}

void SizeDiff::Config::Load(const Log::ConfigSource source)
{
	Settings loaded{};
	std::ifstream in(kIniPath);
	if (!in.good()) {
		spdlog::warn(
			"[CONFIG_LOAD_SOURCE] source={} path={} status=missing_or_unreadable using_defaults=true",
			Log::ToString(source),
			kIniPathLog);
		ApplySettings(loaded, source);
		Save(source);
		return;
	}

	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == ';' || line[0] == '[') {
			continue;
		}
		const auto eq = line.find('=');
		if (eq == std::string::npos) {
			continue;
		}

		const auto key = line.substr(0, eq);
		const auto value = line.substr(eq + 1);
		try {
			if (key == "Mode") {
				const auto parsed = std::stoi(value);
				const auto clamped = std::clamp(parsed, 0, 3);
				loaded.mode = static_cast<Mode>(clamped);
				if (parsed != clamped) {
					spdlog::warn(
						"[CONFIG_VALUE_COERCED] source={} key=Mode inputValue={} finalValue={} reason=clamp",
						Log::ToString(source),
						parsed,
						clamped);
				}
			} else if (key == "Tolerance") {
				const auto parsed = std::stof(value);
				const auto clamped = std::clamp(parsed, 0.0F, 0.50F);
				loaded.tolerance = clamped;
				if (!NearlyEqual(parsed, clamped)) {
					spdlog::warn(
						"[CONFIG_VALUE_COERCED] source={} key=Tolerance inputValue={} finalValue={} reason=clamp",
						Log::ToString(source),
						parsed,
						clamped);
				}
			} else if (key == "ApplyToPlayerScenes") {
				loaded.applyToPlayerScenes = ParseBool(value);
			} else if (key == "ApplyToNpcScenes") {
				loaded.applyToNpcScenes = ParseBool(value);
			} else if (key == "ApplyInAutoMode") {
				loaded.applyInAutoMode = ParseBool(value);
			} else if (key == "LogLevel") {
				bool coerced = false;
				const auto parsed = Log::ParseLevel(value, loaded.logLevel, &coerced);
				loaded.logLevel = parsed;
				if (coerced) {
					spdlog::warn(
						"[CONFIG_VALUE_COERCED] source={} key=LogLevel inputValue={} finalValue={} reason=parse_fallback",
						Log::ToString(source),
						value,
						Log::ToString(parsed));
				}
			}
		} catch (const std::exception& e) {
			spdlog::warn(
				"[CONFIG_VALUE_COERCED] source={} key={} inputValue={} finalValue=default reason=parse_exception error={}",
				Log::ToString(source),
				key,
				value,
				e.what());
		}
	}

	ApplySettings(loaded, source);
	const auto mode = static_cast<int>(loaded.mode);

	spdlog::info(
		"[CONFIG_LOADED] source={} mode={} tolerance={} applyPlayer={} applyNpc={} autoMode={} logLevel={}",
		Log::ToString(source),
		mode,
		loaded.tolerance,
		loaded.applyToPlayerScenes,
		loaded.applyToNpcScenes,
		loaded.applyInAutoMode,
		Log::ToString(loaded.logLevel));
}

void SizeDiff::Config::Save(const Log::ConfigSource source)
{
	Settings snapshot{};
	{
		std::scoped_lock lock(g_mutex);
		snapshot = g_settings;
	}

	const std::filesystem::path iniPath(kIniPath);
	const auto parentPath = iniPath.parent_path();
	if (!parentPath.empty()) {
		std::error_code ec{};
		std::filesystem::create_directories(parentPath, ec);
		if (ec) {
			spdlog::error(
				"[CONFIG_PERSIST_RESULT] source={} path={} result=failed error=create_directories_failed details={}",
				Log::ToString(source),
				kIniPathLog,
				ec.message());
			return;
		}
	}

	std::ofstream out(iniPath);
	if (!out.good()) {
		spdlog::error(
			"[CONFIG_PERSIST_RESULT] source={} path={} result=failed error=could_not_open",
			Log::ToString(source),
			kIniPathLog);
		return;
	}
	out << "[General]\n";
	out << "Mode=" << static_cast<int>(snapshot.mode) << '\n';
	out << "Tolerance=" << snapshot.tolerance << '\n';
	out << "ApplyToPlayerScenes=" << (snapshot.applyToPlayerScenes ? "true" : "false") << '\n';
	out << "ApplyToNpcScenes=" << (snapshot.applyToNpcScenes ? "true" : "false") << '\n';
	out << "ApplyInAutoMode=" << (snapshot.applyInAutoMode ? "true" : "false") << '\n';
	out << "LogLevel=" << Log::ToString(snapshot.logLevel) << '\n';
	spdlog::info(
		"[CONFIG_PERSIST_RESULT] source={} path={} result=success",
		Log::ToString(source),
		kIniPathLog);
}

void SizeDiff::Config::Set(Settings settings)
{
	ApplySettings(settings, std::nullopt);
}

void SizeDiff::Config::SetFromSource(Settings settings, const Log::ConfigSource source)
{
	bool changed = false;
	{
		std::scoped_lock lock(g_mutex);
		changed = !SettingsEqual(g_settings, settings);
	}
	ApplySettings(settings, source);
	if (changed && source == Log::ConfigSource::UI) {
		Save(source);
	}
}

void SizeDiff::Config::Reload()
{
	Load(Log::ConfigSource::DiskReload);
}

SizeDiff::Config::Settings SizeDiff::Config::Get()
{
	std::scoped_lock lock(g_mutex);
	return g_settings;
}

SizeDiff::Config::Mode SizeDiff::Config::GetMode()
{
	return Get().mode;
}

float SizeDiff::Config::GetTolerance()
{
	return Get().tolerance;
}
