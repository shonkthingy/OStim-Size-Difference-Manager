cmake_minimum_required(VERSION 3.21)
project(
    OStimSizeDifferenceManager
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE SKYRIM)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CommonLibPath "${CMAKE_CURRENT_SOURCE_DIR}/../extern/CommonLibSSE-NG")
if (NOT EXISTS "${CommonLibPath}/CMakeLists.txt")
    set(CommonLibPath "E:/Cursor Projects/OStimNG/skse/extern/CommonLibSSE-NG")
endif ()
if (EXISTS "${CommonLibPath}/CMakeLists.txt")
    add_subdirectory("${CommonLibPath}" CommonLibSSE EXCLUDE_FROM_ALL)
else ()
    message(FATAL_ERROR "CommonLibSSE-NG not found. Expected submodule at ../extern/CommonLibSSE-NG")
endif ()

find_package(minhook CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/Main.cpp
    src/Config/Config.cpp
    src/SceneCache/SceneCache.cpp
    src/SceneCache/SceneLoader.cpp
    src/Matching/HeightMatcher.cpp
    src/AddressResolution/VersionGate.cpp
    src/AddressResolution/PdbResolver.cpp
    src/AddressResolution/PatternScanner.cpp
    src/Hooks/GetRandomNodeHook.cpp
    src/Hooks/ThreadContextHook.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if (TARGET CommonLibSSE::CommonLibSSE)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            CommonLibSSE::CommonLibSSE
            minhook::minhook
            spdlog::spdlog
            nlohmann_json::nlohmann_json)
else ()
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            minhook::minhook
            spdlog::spdlog
            nlohmann_json::nlohmann_json)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE Dbghelp Version)

set(OUTPUT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../data")
set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
add_custom_command(
    TARGET "${PROJECT_NAME}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    VERBATIM)
