# OStim Size Difference Manager

An [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/62852) plugin for **Skyrim Special Edition / Anniversary Edition** that integrates with **[OStim NG](https://www.nexusmods.com/skyrimspecialedition/mods/76724)** to filter scene and animation selection based on the actual height difference between actors in a scene.

Scenes that were authored for same-height actors are hidden or skipped when the participating actors have a meaningful size difference — keeping immersion intact without requiring changes to OStim itself.

---

## Requirements

| Requirement | Notes |
|---|---|
| **Skyrim SE / AE** | Both supported via Address Library |
| **[SKSE64](https://www.nexusmods.com/skyrimspecialedition/mods/62852)** | Required |
| **[OStim NG](https://www.nexusmods.com/skyrimspecialedition/mods/76724)** | Tested with **7.4.0.3**; see *OStim Version Support* below |
| **[Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)** | Required |

If `OStim.dll` is not present, the plugin loads but stays completely idle. If the OStim version is unrecognised, hooks are not installed and the plugin will log a warning.

---

## Installation

1. Copy `data/SKSE/Plugins/OStimSizeDifferenceManager.dll` into your `Data/SKSE/Plugins/` folder (or install via your mod manager).
2. Optionally copy `data/SKSE/Plugins/OStimSizeDifferenceManager.ini` to the same location to configure behaviour.
3. Launch the game via SKSE.

Logs are written to: `My Games/Skyrim Special Edition/SKSE/OStimSizeDifferenceManager.log`

---

## Configuration

INI path: `Data/SKSE/Plugins/OStimSizeDifferenceManager.ini`

| Key | Values | Default | Notes |
|---|---|---|---|
| `Mode` | `0` = Off, `1` = Soft (not yet implemented), `2` = Strict | `2` | Controls filtering behaviour |
| `Tolerance` | float (e.g. `0.1`) | `0.1` | Allowed deviation between runtime height spread and a scene's authored scale difference |

Additional keys (`ApplyToPlayerScenes`, `ApplyToNpcScenes`, `ApplyInAutoMode`, `FallbackBehavior`) are parsed but **not yet connected** — reserved for future work.

---

## What It Does

### Scene filtering

On game data load, the plugin scans all OStim scene JSON files under `Data/SKSE/Plugins/OStim/scenes/` and builds a cache of each scene's authored actor scale information.

For each actor in a scene, the plugin reads the optional `scale` field (defaults to `1.0`). The **authored size difference** for a scene is `max(scale) - min(scale)` across all actor slots.

At runtime, when OStim needs to pick or progress a scene, the plugin computes each actor's **visual scale** as:

```
race height × refScale
```

The **runtime spread** is `max - min` across all actors in the current thread.

A scene is allowed when: `|runtime spread − authored difference| ≤ Tolerance`

Scenes with no `scale` data (all actors default to `1.0`) get an authored difference of `0.0`, so they are only approved when actors are similarly sized.

Unknown scenes (not found in the cache) are also treated as authored difference `0.0` — they are rejected when actors have a meaningful size difference.

### Filtering scope

The plugin intercepts three call sites inside OStim NG via [MinHook](https://github.com/TsudaKageyu/minhook):

| Hook | Where it applies |
|---|---|
| `Graph::GraphTable::getRandomNode` | Initial / autonomous scene selection |
| `Graph::Node::getRandomNodeInRange` | Automatic scene progression (AI-driven) |
| `Graph::Navigation::fulfilledBy` | **Player-controlled UI menus** (base OStim UI and OStim Prism) |

The `fulfilledBy` hook filters what appears in the navigation menu — incompatible scenes are hidden before they are displayed, so the player cannot manually select them either.

---

## Current Limitations

### Navigation-only / hub nodes lack scale metadata

Some scenes act purely as navigation hubs or position-change intermediaries (e.g. `ostim2psittingmf`, standing-apart transitions). These do not carry per-actor `scale` data in their JSON, so the plugin treats them as authored difference `0.0`.

**Effect:** If the participating actors have a meaningful height difference and the hub's authored difference is `0.0`, the hub node is filtered — meaning the player may not be able to navigate back to the scene browser from inside a size-difference scene. This is non-gamebreaking (the scene continues) but limits navigation flexibility.

**Planned fix:** Detect and exempt pure transition/hub nodes from the size-difference filter so navigation control is always available.

### Soft mode not implemented

`Mode=1` is reserved but has no distinct behaviour yet — it falls through to the same path as `Mode=0` (off). Strict mode (`Mode=2`) is the only active filtering path.

### OStim version support is manual

Hook addresses are resolved by byte pattern from `signatures.json`. New OStim builds require a pattern update before hooks will install. See *OStim Version Support* below.

### INI settings only — no in-game UI

There is currently no MCM or in-game settings panel. All configuration is done via the INI file and requires a game restart to take effect.

---

## Next Steps

- [ ] **Exempt hub/transition nodes** from size-difference filtering so the player can always navigate back
- [ ] **Implement Soft mode** — allow scenes with a small mismatch rather than hard-rejecting them
- [ ] **MCM / in-game settings** — configure mode and tolerance without editing the INI file
- [ ] **Broader OStim version support** — automate or simplify pattern extraction for new OStim releases
- [ ] **Wire remaining INI flags** — `ApplyToPlayerScenes`, `ApplyToNpcScenes`, `ApplyInAutoMode`, `FallbackBehavior`
- [ ] **Expand scene metadata coverage** — work with animation authors to ensure hub/transition nodes carry correct scale data

---

## OStim Version Support

Hook addresses are resolved in this order:
1. **PDB symbol lookup** (if debug symbols for `OStim.dll` are available)
2. **Version-specific byte patterns** from `data/SKSE/Plugins/signatures.json`

Currently tested and supported: **7.4.0.0**, **7.4.0.3**

To add support for a new OStim version:
1. Add the four-part version to the known-good list in `skse/src/AddressResolution/VersionGate.cpp`.
2. Extract byte patterns for `getRandomNode`, `getRandomNodeInRange`, and `fulfilledBy` from the new `OStim.dll`.
3. Add matching entries in `signatures.json`.
4. Rebuild, test in-game, and verify hook installation in the SKSE log.

---

## Build (Developer)

### Toolchain

- Windows + **MSVC** (Visual Studio 2022)
- **CMake** 3.21+
- **Ninja**
- **vcpkg** with the `x64-windows-skse` triplet (set `VCPKG_ROOT` env var)
- **CommonLibSSE-NG** at `extern/CommonLibSSE-NG`
- vcpkg packages: `minhook`, `spdlog`, `nlohmann_json` (see `skse/vcpkg.json`)

### Build commands

```powershell
# Release
./build.ps1 -Preset build-release-msvc

# Debug
./build.ps1 -Preset build-debug-msvc
```

The post-build step copies the output DLL to `data/SKSE/Plugins/` automatically.

The plugin logs `_MSC_FULL_VER` and `_MSVC_STL_VERSION` on load to help diagnose ABI mismatches with `OStim.dll`.

---

## License

GPL — matches OStim NG.

---
