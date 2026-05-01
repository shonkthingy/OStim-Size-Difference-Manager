#include "Config/Config.h"

#include "Plugin.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace
{
	std::mutex g_mutex;
	SizeDiff::Config::Settings g_settings{};
	inline constexpr auto kIniPath = L"Data/SKSE/Plugins/OStimSizeDifferenceManager.ini";

	bool _dirty = false;
	bool _saveFailed = false;
	std::chrono::steady_clock::time_point _lastMutation{};
	std::chrono::steady_clock::time_point _lastAutosaveAttempt{};

	constexpr float kToleranceEpsilon = 1e-6F;

	bool SettingsEqual(const SizeDiff::Config::Settings& a, const SizeDiff::Config::Settings& b)
	{
		return a.mode == b.mode && std::fabs(a.tolerance - b.tolerance) <= kToleranceEpsilon &&
			a.applyToPlayerScenes == b.applyToPlayerScenes && a.applyToNpcScenes == b.applyToNpcScenes &&
			a.applyInAutoMode == b.applyInAutoMode;
	}

	void MarkDirtyLocked()
	{
		_dirty = true;
		_lastMutation = std::chrono::steady_clock::now();
	}

	void ClearPersistStateLocked()
	{
		_dirty = false;
		_saveFailed = false;
	}

	std::string BuildIniContent(const SizeDiff::Config::Settings& s)
	{
		std::ostringstream out;
		out << "[General]\n";
		out << "Mode=" << static_cast<int>(s.mode) << '\n';
		out << "Tolerance=" << s.tolerance << '\n';
		out << "ApplyToPlayerScenes=" << (s.applyToPlayerScenes ? "true" : "false") << '\n';
		out << "ApplyToNpcScenes=" << (s.applyToNpcScenes ? "true" : "false") << '\n';
		out << "ApplyInAutoMode=" << (s.applyInAutoMode ? "true" : "false") << '\n';
		return out.str();
	}

	bool WriteIniToDisk()
	{
		std::string content;
		bool hadDirty = false;
		{
			std::scoped_lock lock(g_mutex);
			hadDirty = _dirty;
			if (!hadDirty) {
				return true;
			}
			content = BuildIniContent(g_settings);
		}

		const std::filesystem::path path{ kIniPath };
		const std::filesystem::path tmpPath = path.string() + ".tmp";
		std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
		if (!out.good()) {
			spdlog::error("Config: could not write {}", tmpPath.string());
			std::scoped_lock lock(g_mutex);
			_saveFailed = true;
			return false;
		}
		out << content;
		out.flush();
		if (!out.good()) {
			spdlog::error("Config: could not flush temp file {}", tmpPath.string());
			std::scoped_lock lock(g_mutex);
			_saveFailed = true;
			return false;
		}
		out.close();

		std::error_code ec;
		std::filesystem::rename(tmpPath, path, ec);
		if (ec) {
			std::filesystem::remove(path, ec);
			ec.clear();
			std::filesystem::rename(tmpPath, path, ec);
			if (ec) {
				spdlog::error("Config: could not atomically replace {}: {}", path.string(), ec.message());
				std::scoped_lock lock(g_mutex);
				_saveFailed = true;
				return false;
			}
		}
		spdlog::info("Config saved to OStimSizeDifferenceManager.ini");
		{
			std::scoped_lock lock(g_mutex);
			_dirty = false;
			_saveFailed = false;
		}
		return true;
	}
}

void SizeDiff::Config::Load()
{
	int modeForLog = 0;
	float toleranceForLog = 0.0F;
	bool applyPlayerForLog = false;
	bool applyNpcForLog = false;
	bool autoModeForLog = false;

	{
		std::scoped_lock lock(g_mutex);

		std::ifstream in(kIniPath);
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
			if (key == "Mode") {
				g_settings.mode = static_cast<Mode>(std::clamp(std::stoi(value), 0, 3));
			} else if (key == "Tolerance") {
				g_settings.tolerance = std::clamp(std::stof(value), 0.0F, 0.50F);
			} else if (key == "ApplyToPlayerScenes") {
				g_settings.applyToPlayerScenes = (value == "1" || value == "true");
			} else if (key == "ApplyToNpcScenes") {
				g_settings.applyToNpcScenes = (value == "1" || value == "true");
			} else if (key == "ApplyInAutoMode") {
				g_settings.applyInAutoMode = (value == "1" || value == "true");
			}
		}

		ClearPersistStateLocked();

		modeForLog = static_cast<int>(g_settings.mode);
		toleranceForLog = g_settings.tolerance;
		applyPlayerForLog = g_settings.applyToPlayerScenes;
		applyNpcForLog = g_settings.applyToNpcScenes;
		autoModeForLog = g_settings.applyInAutoMode;
	}

	spdlog::info(
		"Config loaded: mode={}, tolerance={}, applyPlayer={}, applyNpc={}, autoMode={}",
		modeForLog,
		toleranceForLog,
		applyPlayerForLog,
		applyNpcForLog,
		autoModeForLog);
}

void SizeDiff::Config::Set(Settings settings)
{
	std::scoped_lock lock(g_mutex);
	if (SettingsEqual(g_settings, settings)) {
		return;
	}
	g_settings = std::move(settings);
	MarkDirtyLocked();
}

void SizeDiff::Config::Reload()
{
	Load();
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

bool SizeDiff::Config::TryAutosave(std::chrono::steady_clock::time_point now, std::chrono::milliseconds debounce)
{
	{
		std::scoped_lock lock(g_mutex);
		if (!_dirty) {
			return true;
		}
		if (now - _lastMutation < debounce || now - _lastAutosaveAttempt < debounce) {
			return false;
		}
		_lastAutosaveAttempt = now;
	}
	return WriteIniToDisk();
}

bool SizeDiff::Config::FlushDirtyNow()
{
	return WriteIniToDisk();
}

SizeDiff::Config::PersistStatus SizeDiff::Config::GetPersistStatus()
{
	std::scoped_lock lock(g_mutex);
	if (_saveFailed) {
		return PersistStatus::SaveFailed;
	}
	return _dirty ? PersistStatus::Dirty : PersistStatus::Saved;
}
