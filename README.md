# OStim Size Difference Manager

SKSE plugin that integrates with OStim at runtime and filters scene selection by authored vs runtime actor scale difference.

## Status

- v1 target: strict mode filtering with tolerance control.
- Soft mode is intentionally stubbed as coming soon.

## Build Requirements

- Visual Studio 2022 (MSVC)
- CMake 3.21+
- vcpkg with `x64-windows-skse` triplet support
- CommonLibSSE-NG at `extern/CommonLibSSE-NG`

The plugin logs `_MSC_FULL_VER` and `_MSVC_STL_VERSION` on load to diagnose ABI mismatches.

## Build

```powershell
./build.ps1 -Preset build-release-msvc
```

Output DLL is copied to `data/SKSE/Plugins`.

## Runtime Safety

- If `OStim.dll` is missing, plugin stays idle.
- If OStim version is unknown, plugin logs warning and does not hook.
- If scene cache is not ready yet, scene filtering is permissive.

## M1 Verification

1. Build with `./build.ps1 -Preset build-debug-msvc`.
2. Drop resulting `OStimSizeDifferenceManager.dll` into `Data/SKSE/Plugins`.
3. Launch Skyrim with SKSE and check `My Games/Skyrim Special Edition/SKSE/OStimSizeDifferenceManager.log`.
4. Confirm log contains plugin version + `_MSC_FULL_VER`/`_MSVC_STL_VERSION`.
5. Test without `OStim.dll`: plugin should log warning and stay idle with no crash.
6. Test with `OStim.dll`: plugin should continue to load and register hook/scanner scaffolding.

## Signature Workflow

`data/SKSE/Plugins/signatures.json` maps OStim version to byte signature fallback.

1. Build/obtain the exact OStim target binary.
2. Inspect `Graph::GraphTable::getRandomNode` in a disassembler.
3. Derive stable signature bytes + mask.
4. Add/update entry in `signatures.json`.
5. Test in-game with logs enabled.
