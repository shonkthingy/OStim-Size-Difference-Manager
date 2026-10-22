#!/usr/bin/env bash
# Linux build wrapper for OStim Size Difference Manager.
# Mirrors build.ps1: cross-compiles the MSVC-ABI DLL with clang-cl + lld-link
# against an xwin Windows SDK/CRT sysroot. See README.md "Toolchain" for the
# one-time setup (xwin splat, vcpkg, llvm-mingw + Wine for the directxtk port).
set -euo pipefail

Preset="${1:-build-release-linux-clangcl}"
case "$Preset" in
    *debug*) BuildPreset="debug-linux-clangcl" ;;
    *)       BuildPreset="release-linux-clangcl" ;;
esac

fail() { echo "error: $*" >&2; exit 1; }

for tool in cmake ninja clang-cl lld-link llvm-rc llvm-mt; do
    command -v "$tool" >/dev/null 2>&1 || fail "'$tool' not found on PATH. See README.md 'Toolchain' (Linux) for setup."
done

# Windows SDK + MSVC CRT sysroot staged by xwin. The toolchain file defaults to
# ~/xwin-out as well; export XWIN_SYSROOT to override.
export XWIN_SYSROOT="${XWIN_SYSROOT:-$HOME/xwin-out}"
[ -d "$XWIN_SYSROOT" ] || fail "xwin sysroot not found at $XWIN_SYSROOT. Run: xwin --accept-license splat --output ~/xwin-out --include-debug-libs --use-winsysroot-style --preserve-ms-arch-notation"

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}"
[ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ] || fail "vcpkg not found at $VCPKG_ROOT (set VCPKG_ROOT or clone+bootstrap vcpkg there)"

# Only needed to build the fxc2 shader-compiler stand-in for the directxtk port.
if ! command -v x86_64-w64-mingw32-clang++ >/dev/null 2>&1 && [ -z "${LLVM_MINGW_BIN:-}" ]; then
    if [ -x "$HOME/tools/llvm-mingw/bin/x86_64-w64-mingw32-clang++" ]; then
        export LLVM_MINGW_BIN="$HOME/tools/llvm-mingw/bin"
    else
        fail "x86_64-w64-mingw32-clang++ (llvm-mingw) not found; needed to build the directxtk shader workaround. See README.md 'Toolchain' (Linux)."
    fi
fi
command -v wine >/dev/null 2>&1 || echo "warning: 'wine' not found; the directxtk shader step will fail" >&2

ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SkseDir="$(cd "$ScriptDir/skse" && pwd)"

# Submodules must be initialized recursively: CommonLibSSE-NG vendors openvr,
# whose openvr_api.lib the link step requires.
[ -f "$ScriptDir/extern/CommonLibSSE-NG/extern/openvr/lib/win64/openvr_api.lib" ] || \
    fail "submodules not initialized (missing extern/CommonLibSSE-NG/extern/openvr). Run: git submodule update --init --recursive"

(cd "$SkseDir" && cmake --preset "$Preset" && cmake --build --preset "$BuildPreset")
