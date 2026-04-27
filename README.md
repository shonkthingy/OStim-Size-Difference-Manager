# OStim Size Difference Manager

An [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/62852) plugin for **Skyrim Special Edition / Anniversary Edition** that integrates with **[OStim NG](https://www.nexusmods.com/skyrimspecialedition/mods/76724)** to filter scene and animation selection based on the actual height difference between actors in a scene.

Scenes that were authored for same-height actors are hidden or skipped when the participating actors have a meaningful size difference — keeping immersion intact without requiring changes to OStim itself.

**1.0 highlights**

- **Filtering Mode (4 states):** **Off** (no filtering), **Strict** (hard filter only matching scenes), **Soft** (try Strict first; if nothing matches, fall back to the closest match), and **Debug** (log decisions but do not block scenes). Together, Strict and Soft let you run a hard size match or a “best available” fallback.
- **SKSE Menu UI:** Search filters (including dual search for pack vs animation), hide-empty-packs, inline override inputs, pack-level mass actions, and a cleaner layout—browse packs, set exemptions and overrides, and adjust **Mode** without leaving the game.
- **Live hooks:** Random-node and auto-progression paths pick up INI and menu changes while you play; no full game restart needed when you change filtering behaviour.

---

## Requirements

| Requirement | Notes |
|---|---|
| **Skyrim SE / AE** | Both supported via Address Library |
| **[SKSE64](https://www.nexusmods.com/skyrimspecialedition/mods/62852)** | Required |
| **[OStim NG](https://www.nexusmods.com/skyrimspecialedition/mods/76724)** | Tested with **7.4.0.3**; see *How it works* below |
| **[Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)** | Required |
| **[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/100062)** | *Soft Requirement* for the in-game settings menu. (Configurable via INI without it). |
| **Size-Difference Animations** | *Soft Requirement*. This mod filters scenes so that size-difference actors only use size-difference animations. You will need actual OStim animation packs designed for different sized actors installed for this to be useful. |

If `OStim.dll` is not present, the plugin loads but stays completely idle. If the OStim version is unrecognised, hooks are not installed and the plugin will log a warning.

---

## Installation

1. Install all hard requirements listed above.
2. Download the latest release `.zip` archive. The package mirrors the **Skyrim `Data/` layout** (e.g. `Data/SKSE/Plugins/…`); install so those paths merge into your game’s `Data` folder.
3. Install via your mod manager (Vortex/MO2), or manually extract the contents into your Skyrim **`Data/`** directory (not the game root by itself—`Data` is the merge target).
4. **Crucial Step:** Open OStim's in-game Alignment settings and turn **OFF** OStim's built-in auto-scaling feature. This mod is designed to work with actors at their natural persistent scales.

Logs are written to: `My Games/Skyrim Special Edition/SKSE/OStimSizeDifferenceManager.log`

---

## Configuration

**Filtering Mode** is a **4-state** system (also labelled **Mode** in the INI and menu):

| State | INI `Mode` | Behaviour |
| --- | --- | --- |
| **Off** | `0` | No size filtering. |
| **Strict** | `1` | Hard filter: only scenes within tolerance. |
| **Soft** | `2` | Two-pass: Strict first; if no match, **closest match** fallback. |
| **Debug** | `3` | Log filtering logic but allow any scene. |

Settings can be changed dynamically in-game via the **SKSE Menu Framework**, or by manually editing `Data/SKSE/Plugins/OStimSizeDifferenceManager.ini`. With the menu installed, these options apply on the fly together with the hooks below (no restart required for **Mode** / tolerance / scope toggles in normal use).

| Option | Default | Description |
|---|---|---|
| **Mode** | `1` (Strict) | The four **Filtering Mode** values above. See in-game tooltips. |
| **Tolerance** | `0.1` | Max allowed deviation between actors' actual height spread and the scene's authored spread. |
| **ApplyToPlayerScenes** | `True` | Filter scenes involving the player character. |
| **ApplyToNpcScenes** | `True` | Filter scenes only involving NPCs. |
| **ApplyInAutoMode** | `True` | Filter scenes during OStim's automatic progression. |

### Exemptions & Overrides
The SKSE Menu Framework includes a dedicated tab to visually browse all installed animation packs. From there, you can:
- **Exempt** complete packs or individual scenes (allowing them to play regardless of height difference).
- **Override** the authored scale difference of a specific animation if the mod author tagged it incorrectly.

These settings are saved to `Data/SKSE/Plugins/OStimSizeDifferenceManager_Overrides.json`.

---

## Limitations

### Multi-actor (3+) scenes
3+ person animations are untested, and dedicated functionality for them needs to be worked on.

### INI settings only without SKSE Menu Framework
If you do not install SKSE Menu Framework, there is no in-game settings panel. All configuration is done via the INI file and requires a game restart to take effect.

### Ratio vs Absolute Differences
Currently, the mod calculates size differences as an absolute value (e.g. `Actor 1 (1.0) - Actor 2 (0.8) = 0.2`). This means the mod cannot currently differentiate between a "Larger Dom" and a "Larger Sub". An animation authored for a Large Sub may incorrectly play for a Large Dom if the absolute difference is exactly the same.

### Disabled OStim Auto-Scaling
As noted in the installation steps, OStim's auto-scaling feature is not fully supported and should be turned off.

---

## ToDo

A full list of follow-up work, edge cases, and future ideas is in `TODO.md`. **1.0** delivers the 4-state Filtering Mode (including **Soft** closest-match), the SKSE menu UX overhaul, and live hook behaviour. Still on the radar, for example:

- Ratio-based matching (vs absolute difference) and Dom/Sub role awareness.
- Broader OStim auto-scaling compatibility.
- Hub/transition fixes, 3+ actor support, and maintenance items as listed in `TODO.md`.

---

## How It Works

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

Unknown scenes (not found in the cache) are also treated as authored difference `0.0` — they are rejected when actors have a meaningful size difference. Base OStim hub scenes (prefixed with `ostim`) bypass this logic natively to preserve UI navigation.

### Filtering scope

The plugin intercepts three call sites inside OStim NG via [MinHook](https://github.com/TsudaKageyu/minhook):

| Hook | Where it applies |
|---|---|
| `Graph::GraphTable::getRandomNode` | Initial / autonomous scene selection |
| `Graph::Node::getRandomNodeInRange` | Automatic scene progression (AI-driven) |
| `Graph::Navigation::fulfilledBy` | **Player-controlled UI menus** (base OStim UI and OStim Prism) |

These call sites use the current **Filtering Mode**, tolerance, and scope when they run, so **random** picks and **auto-progression** reflect INI and menu changes **without restarting the game**.

The `fulfilledBy` hook filters what appears in the navigation menu — incompatible scenes are hidden before they are displayed, so the player cannot manually select them either.

### OStim Version Support

Hook addresses are resolved in this order:
1. **PDB symbol lookup** (if debug symbols for `OStim.dll` are available)
2. **Version-specific byte patterns** from `data/SKSE/Plugins/signatures.json`

Currently tested and supported: **7.4.0.0**, **7.4.0.3**

To add support for a new OStim version, its version string must be whitelisted in `skse/src/AddressResolution/VersionGate.cpp` and its byte patterns added to `signatures.json`.

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

GPL 3.0
