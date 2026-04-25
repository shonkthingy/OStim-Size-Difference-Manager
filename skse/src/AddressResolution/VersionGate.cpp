#include "AddressResolution/VersionGate.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

namespace
{
	const std::array kKnownVersions{
		"7.4.0.0",
		"7.4.0.3"
	};
}

std::optional<std::string> SizeDiff::AddressResolution::GetOStimVersionString()
{
	const auto h = GetModuleHandleA("OStim.dll");
	if (!h) {
		return std::nullopt;
	}

	char modulePath[MAX_PATH]{};
	if (!GetModuleFileNameA(h, modulePath, MAX_PATH)) {
		return std::nullopt;
	}

	DWORD handle = 0;
	const auto size = GetFileVersionInfoSizeA(modulePath, &handle);
	if (size == 0) {
		return std::nullopt;
	}

	std::vector<char> versionData(size);
	if (!GetFileVersionInfoA(modulePath, 0, size, versionData.data())) {
		return std::nullopt;
	}

	VS_FIXEDFILEINFO* info = nullptr;
	UINT infoSize = 0;
	if (!VerQueryValueA(versionData.data(), "\\", reinterpret_cast<LPVOID*>(&info), &infoSize) || !info) {
		return std::nullopt;
	}

	return std::format("{}.{}.{}.{}",
		HIWORD(info->dwFileVersionMS),
		LOWORD(info->dwFileVersionMS),
		HIWORD(info->dwFileVersionLS),
		LOWORD(info->dwFileVersionLS));
}

bool SizeDiff::AddressResolution::IsKnownGoodVersion(const std::string& version)
{
	for (const auto* known : kKnownVersions) {
		if (version == known) {
			return true;
		}
	}
	spdlog::warn("Unknown OStim.dll version '{}'; plugin will run in no-hook mode", version);
	return false;
}
