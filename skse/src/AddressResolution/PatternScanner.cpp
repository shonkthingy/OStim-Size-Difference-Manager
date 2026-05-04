#include "AddressResolution/PatternScanner.h"

#include "spdlog/spdlog.h"

#include <Windows.h>
#include <sstream>
#include <vector>

namespace
{
    // Parse a pattern string like "48 89 ? 5C" into bytes + mask
    struct PatternByte { uint8_t value; bool wildcard; };

    std::vector<PatternByte> ParsePattern(const std::string& pattern)
    {
        std::vector<PatternByte> result;
        std::istringstream ss(pattern);
        std::string token;
        while (ss >> token) {
            if (token == "?" || token == "??") {
                result.push_back({ 0, true });
            } else {
                try {
                    result.push_back({ static_cast<uint8_t>(std::stoul(token, nullptr, 16)), false });
                } catch (const std::exception& e) {
                    spdlog::error("[PATTERN_PARSE_FAIL] token={} error={}", token, e.what());
                    return {};
                }
            }
        }
        return result;
    }

    // Scan [start, start+size) for the pattern
    const uint8_t* ScanRegion(const uint8_t* start, std::size_t size, const std::vector<PatternByte>& pattern)
    {
        if (pattern.empty() || size < pattern.size()) return nullptr;
        const std::size_t patLen = pattern.size();
        const uint8_t* end = start + size - patLen;
        for (const uint8_t* p = start; p <= end; ++p) {
            bool match = true;
            for (std::size_t i = 0; i < patLen; ++i) {
                if (!pattern[i].wildcard && p[i] != pattern[i].value) {
                    match = false;
                    break;
                }
            }
            if (match) return p;
        }
        return nullptr;
    }
}

std::optional<std::uintptr_t> SizeDiff::AddressResolution::ResolveByPattern(const PatternDefinition& def)
{
    const auto moduleBase = reinterpret_cast<const uint8_t*>(GetModuleHandleA("OStim.dll"));
    if (!moduleBase) {
        spdlog::warn("PatternScanner: OStim.dll not loaded");
        return std::nullopt;
    }

    if (def.pattern.empty() || def.pattern == "TODO_PATTERN") {
        spdlog::warn("PatternScanner: no valid pattern for version '{}'", def.version);
        return std::nullopt;
    }

    // Walk PE sections looking for .text
    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(moduleBase + dosHeader->e_lfanew);
    const auto* section = IMAGE_FIRST_SECTION(ntHeaders);

    const auto parsed = ParsePattern(def.pattern);
    if (parsed.empty()) {
        spdlog::error("PatternScanner: could not parse pattern for version '{}'", def.version);
        return std::nullopt;
    }

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        // Scan executable sections (typically .text)
        if (!(section->Characteristics & IMAGE_SCN_CNT_CODE)) continue;

        const auto* regionStart = moduleBase + section->VirtualAddress;
        const std::size_t regionSize = section->Misc.VirtualSize;

        const uint8_t* match = ScanRegion(regionStart, regionSize, parsed);
        if (match) {
            const auto address = reinterpret_cast<std::uintptr_t>(match);
            spdlog::debug("PatternScanner: found '{}' at 0x{:X} (section {})",
                def.version, address,
                std::string(reinterpret_cast<const char*>(section->Name), 8));
            return address;
        }
    }

    spdlog::warn("PatternScanner: pattern not found for version '{}' (pattern: {})", def.version, def.pattern);
    return std::nullopt;
}
