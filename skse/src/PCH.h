#pragma once

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REL/Relocation.h>

#include <ShlObj_core.h>
#include <Windows.h>
#include <Psapi.h>
#include <DbgHelp.h>
#undef cdecl  // Workaround for Clang 14 CMake configure error.

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <nlohmann/json.hpp>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;
using namespace REL::literals;

namespace logger = SKSE::log;
namespace fs = std::filesystem;
using json = nlohmann::json;
