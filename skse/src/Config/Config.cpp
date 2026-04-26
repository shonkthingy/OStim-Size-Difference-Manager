#include "Config/Config.h"

#include "Plugin.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <fstream>
#include <mutex>

namespace
{
	std::mutex g_mutex;
	SizeDiff::Config::Settings g_settings{};
	inline constexpr auto kIniPath = L"Data/SKSE/Plugins/OStimSizeDifferenceManager.ini";
}

void SizeDiff::Config::Load()
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
			g_settings.mode = static_cast<Mode>(std::clamp(std::stoi(value), 0, 2));
		} else if (key == "Tolerance") {
			g_settings.tolerance = std::clamp(std::stof(value), 0.0F, 0.50F);
		} else if (key == "ApplyToPlayerScenes") {
			g_settings.applyToPlayerScenes = (value == "1" || value == "true");
		} else if (key == "ApplyToNpcScenes") {
			g_settings.applyToNpcScenes = (value == "1" || value == "true");
		} else if (key == "ApplyInAutoMode") {
			g_settings.applyInAutoMode = (value == "1" || value == "true");
		} else if (key == "FallbackBehavior") {
			g_settings.fallbackBehavior = std::clamp(std::stoi(value), 0, 2);
		}
	}

	const auto mode = static_cast<int>(g_settings.mode);

	spdlog::info(
		"Config loaded: mode={}, tolerance={}, applyPlayer={}, applyNpc={}, autoMode={}, fallback={}",
		mode,
		g_settings.tolerance,
		g_settings.applyToPlayerScenes,
		g_settings.applyToNpcScenes,
		g_settings.applyInAutoMode,
		g_settings.fallbackBehavior);
}

void SizeDiff::Config::Save()
{
	std::scoped_lock lock(g_mutex);
	std::ofstream out(kIniPath);
	if (!out.good()) {
		spdlog::error("Config: could not write {}", "Data/SKSE/Plugins/OStimSizeDifferenceManager.ini");
		return;
	}
	out << "[General]\n";
	out << "Mode=" << static_cast<int>(g_settings.mode) << '\n';
	out << "Tolerance=" << g_settings.tolerance << '\n';
	out << "ApplyToPlayerScenes=" << (g_settings.applyToPlayerScenes ? "true" : "false") << '\n';
	out << "ApplyToNpcScenes=" << (g_settings.applyToNpcScenes ? "true" : "false") << '\n';
	out << "ApplyInAutoMode=" << (g_settings.applyInAutoMode ? "true" : "false") << '\n';
	out << "FallbackBehavior=" << g_settings.fallbackBehavior << '\n';
	spdlog::info("Config saved to OStimSizeDifferenceManager.ini");
}

void SizeDiff::Config::Set(Settings settings)
{
	std::scoped_lock lock(g_mutex);
	g_settings = std::move(settings);
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
