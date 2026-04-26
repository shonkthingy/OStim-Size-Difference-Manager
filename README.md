# OStim Size Difference Manager

An [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/62852) plugin for **Skyrim Special Edition / Anniversary Edition** that integrates with **[OStim NG](https://www.nexusmods.com/skyrimspecialedition/mods/76724)**. It **restricts which scenes OStim may pick**—both for initial selection and for automatic scene progression—so that the **height spread among actors at runtime** lines up with what each scene was authored for.

In practice, that means autonomous animations and auto-progression steps that would pair mismatched body sizes (for example, a very tall character with a very short one) are filtered out unless the scene explicitly allows that size difference in its data.

---

## How it works (implementation)

### Hooks (MinHook)

OStim’s scene graph picks nodes through two call sites. The plugin intercepts both with [MinHook](https://github.com/TsudaKageyu/minhook):

| Hook | OStim API role |
|------|----------------|
| `Graph::GraphTable::getRandomNode` | **Entry / initial scene selection** — choosing a node from the top-level graph. |
| `Graph::Node::getRandomNodeInRange` | **Auto-progression (and related navigation)** — moving along the graph by distance. |

Resolution order for each address: **PDB symbol lookup** (when debug symbols for `OStim.dll` are available), else **version-specific byte patterns** (see `data/SKSE/Plugins/signatures.json`). If the OStim build is not a known, tested version, the plugin **does not install these hooks** (avoids undefined behavior on unknown binaries).

### Runtime “height” (visual scale per actor)

For each actor in the active OStim thread, the plugin computes a **single scale value**:

**race height × refScale**

- **Race height** comes from the actor’s `TESRace` data (`height` for the actor’s sex).
- **refScale** is the reference scale from the actor (stored as 0–100 in the game; the plugin uses it as a 0.0–1.0 float factor).

So shorter races, scaled NPCs, and similar cases are represented by **real in-game proportions**, not a flat 1.0 for everyone.

The **spread** used for decisions is the **max − min** of those values across all actors in the current thread.

OStim’s public **thread** interface is used to register listeners (thread start, node change, thread stop) so the plugin knows which actors and scales apply when `getRandomNode` / `getRandomNodeInRange` run. That context is passed into the hook logic via thread-local storage.

### Scene database (`SceneCache`)

On **data load**, a **background thread** walks:

`Data/SKSE/Plugins/OStim/scenes/**/*.json`

For each valid scene file it:

1. Resolves a **scene id** (lowercased `id` from JSON if present, else the file stem).
2. Reads the **`actors`** array and each actor’s optional **`scale`** (defaults to **1.0** if omitted).
3. Records **min / max scale** and an **authored size difference** = `max(scale) − min(scale)` for that scene.

**Filtering logic (Strict mode):**

- If the scene id **is in the cache** (parsed from OStim’s JSON): a scene is allowed when the **runtime** max−min scale is **within tolerance** of the **authored** difference derived from per-actor `scale` in that file:  
  `|runtimeDiff − authoredDiff| ≤ Tolerance` (see INI).
- If the scene id is **not** in the map (e.g. **unknown** to the cache because of an id mismatch): the code only allows the candidate when **runtime spread ≤ Tolerance**—i.e. it behaves like an **authored difference of 0** for matching purposes. With **tolerance 0** (or very small), **mismatched actor groups are rejected** for those ids.

Scenes whose JSON only specifies **default `scale` (1.0) for every slot** get an **authored difference of 0**, so they only fit parties with **negligible height spread** (within `Tolerance`), unless you widen tolerance globally.

**While the background scan is still running**, the cache is not marked ready; matching **allows** candidates (permissive) until indexing completes, so load order does not hard-block the game.

**Soft mode** is **not implemented** yet: only **Strict** applies the wrapping logic; other modes do not perform this filtering in the current build.

---

## Current status

- **Strict mode:** Implemented and, in current testing, **working as intended** for both initial selection and auto-progression, using the hooks and cache described above.
- **Soft mode:** **Unimplemented** (reserved in configuration; no separate behavior in code yet).
- **OStim NG builds:** Hook installation is **enabled** only for **known** `OStim.dll` file versions the project has been validated against (e.g. **7.4.0.0**, **7.4.0.3**). Other versions load the plugin but **skip hooks** until support is added (see *Signatures and new OStim versions*).

---

## Requirements (runtime)

| Requirement | Notes |
|-------------|--------|
| **Skyrim SE/AE** | Target runtime uses Address Library. |
| **[SKSE64](https://www.nexusmods.com/skyrimspecialedition/mods/62852)** | Required. |
| **OStim NG** | Tested with **7.4.0.3** (and listed compatible builds in source); other versions need verification. |
| **Address Library for SKSE** | Required by the plugin’s SKSE version metadata. |

If `OStim.dll` is not present, the plugin **loads but stays idle** (no OStim hooking). If the OStim version is not recognized, the plugin **does not** install graph hooks.

Log file (typical): `My Games/Skyrim Special Edition/SKSE/OStimSizeDifferenceManager.log` (exact path may vary with your setup). The plugin logs MSVC/STL build identifiers on load to help catch ABI mismatches with `OStim.dll`.

### Configuration

INI path (under the game’s `Data` tree): `SKSE/Plugins/OStimSizeDifferenceManager.ini`

**Behavior today is driven by** `Mode` and `Tolerance`:

- **`Mode`:** `0` = Off, `1` = Soft (not implemented yet; behaves like no scale filtering in the current build), `2` = **Strict** (applies the hooks described above).
- **`Tolerance`:** Allowed deviation when comparing **runtime** max−min scale to a scene’s **authored** difference (or when treating unknown scene ids; clamped in code, typically a small value such as `0.0`–`0.1`).

Additional keys (`ApplyToPlayerScenes`, `ApplyToNpcScenes`, `ApplyInAutoMode`, `FallbackBehavior`) are **read from the INI** but are **not yet connected** to the hook path in this version—they are reserved for future work.

---

## Build (developer)

### Toolchain

- **Windows**, **MSVC** (e.g. Visual Studio 2022)
- **CMake** 3.21+
- **Ninja** (presets use the Ninja generator)
- **vcpkg** with the project’s **SKSE** triplet (see `skse/CMakePresets.json`: `x64-windows-skse`, toolchain from `%VCPKG_ROOT%`)
- **CommonLibSSE-NG** at `extern/CommonLibSSE-NG` (or the path expected by `skse/CMakeLists.txt`)
- vcpkg packages used by the plugin: **minhook**, **spdlog**, **nlohmann_json** (see `skse/vcpkg.json`)

The post-build step copies the output DLL to `data/SKSE/Plugins/` in this repository.

### Commands

```powershell
./build.ps1 -Preset build-release-msvc
```

For a debug build:

```powershell
./build.ps1 -Preset build-debug-msvc
```

Adjust `-Preset` to match the **configure** preset name in `skse/CMakePresets.json` (e.g. `build-release-msvc` / `build-debug-msvc`).

---

## Signatures and new OStim versions

OStim updates can change code layout. This repo keeps **per-version byte patterns** in `data/SKSE/Plugins/signatures.json` as a fallback when PDB resolution is not used.

To support a new `OStim.dll` build:

1. Add its four-part file version to the known-good list in `skse/src/AddressResolution/VersionGate.cpp` *after* you have confirmed patterns and behavior.
2. If needed, derive stable patterns for `Graph::GraphTable::getRandomNode` and `Graph::Node::getRandomNodeInRange` and add matching entries in `signatures.json`.
3. Rebuild, test in-game, and check the SKSE log for successful hook installation lines.

