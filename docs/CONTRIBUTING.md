# Contributing

Thank you for contributing to OStim Size Difference Manager. This document records the repository conventions for changes, builds, and releases.

## Repository layout

- `skse/src/` — plugin source code.
- `skse/CMakeLists.txt` and `skse/CMakePresets.json` — build configuration.
- `data/` — installable Skyrim `Data/` files that are safe to track.
- `docs/` — contributor and architecture documentation.
- `extern/CommonLibSSE-NG/` — CommonLibSSE-NG submodule, tracked at [alandtse/CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (`ng` branch, pinned to a release tag). It is **not** the upstream CharmedBaryon fork; see `README.md` (Changelog v1.0.6) for why.
- `skse/build/` — local build output; never commit it.

Generated binaries and intermediate files are intentionally ignored. In particular, do not commit:

- `skse/build/`
- `data/SKSE/Plugins/OStimSizeDifferenceManager.dll`
- MSVC artifacts such as `.pdb`, `.obj`, `.lib`, `.exp`, `.ilk`, `.ipdb`, and `.iobj`
- Local release ZIP files

The release DLL belongs in the GitHub Release mod archive, not in the Git repository.

## Branches and commits

Use a short-lived branch for non-trivial work. Keep commits focused and use the established lowercase Conventional Commit style:

```text
feat: add a new filtering mode
fix: handle missing scene metadata
docs: update build instructions
chore: refresh release assets
release: v1.0.4
```

Allowed subject prefixes are:

- `feat:` — new user-visible functionality.
- `fix:` — a bug fix.
- `docs:` — documentation-only changes.
- `chore:` — maintenance, tooling, or repository housekeeping.
- `refactor:` — behavior-preserving code restructuring.
- `test:` — tests or validation changes.
- `release:` — the single commit that prepares a versioned release.

Use an imperative, concise subject, keep it lowercase after the prefix, and do not end the subject with punctuation. A release should normally be one squashed commit named exactly `release: vX.Y.Z`.

Release commits must also include a short body describing the user-visible changes and validation performed. Keep the body focused: use one bullet per meaningful change and mention the release build or smoke test when applicable. For example:

```text
release: v1.0.5

- Recognize OStim PE FileVersion 7.5.0.0 so all filtering hooks install.
- Build and smoke-test the release DLL in-game.
```

Do not leave release commits with only the subject line.

## Versioning

The release version must agree in all of these locations:

1. `skse/src/Plugin.h` — `kPluginVersion`.
2. `skse/CMakeLists.txt` — project `VERSION`.
3. `README.md` — the changelog entry.
4. Git tag — `vX.Y.Z`.
5. GitHub Release title — `vX.Y.Z`.

Use semantic versioning for plugin releases. Update the patch version for compatibility updates and bug fixes unless the change requires a minor or major version increment.

## OStim compatibility updates

When adding support for an OStim build:

1. Record the OStim PE `FileVersion` exactly as four numeric components.
2. Add the version to `kKnownVersions` in `skse/src/AddressResolution/VersionGate.cpp`.
3. Add one signature entry for each hooked symbol to `data/SKSE/Plugins/OStimSizeDifferenceManager-Signatures.json`:
   - `Graph::GraphTable::getRandomNode`
   - `Graph::Node::getRandomNodeInRange`
   - `Graph::Navigation::fulfilledBy`
4. Update hook patterns in the hook installers if the prologue changes.
5. Review `skse/src/OStimTypes/` and mangled PDB names if the OStim ABI changes.
6. Update the OStim version list and changelog in `README.md`.
7. Build the release configuration and perform in-game smoke testing when possible.

The JSON signature manifest is documentation and tooling data; the DLL uses the patterns compiled into the hook installers and does not load the JSON at runtime. Keep both synchronized.

## Building

Build from the repository's `Master/` directory — Windows PowerShell (native MSVC) or Linux (`build.sh`, `clang-cl` cross-compile). Set `VCPKG_ROOT`; on Windows install Visual Studio with the Desktop development with C++ workload, CMake 3.21+, Ninja, and `vswhere`; on Linux see the *Toolchain* section of `README.md` for the one-time setup. Initialize submodules with `git submodule update --init --recursive` before the first build (`extern/CommonLibSSE-NG` vendors `openvr`, required at link time).

```powershell
# Release build — use this for ABI validation and packaging
./build.ps1 -Preset build-release-msvc

# Debug build — compile checks only
./build.ps1 -Preset build-debug-msvc
```

On Linux, use the `clang-cl` cross-compile presets (same MSVC-ABI output; release configuration is still required for ABI validation and packaging):

```bash
# Release build — use this for ABI validation and packaging
./build.sh

# Debug build — compile checks only
./build.sh build-debug-linux-clangcl
```

`build.ps1` initializes the x64 MSVC environment, runs vcpkg/CMake, and copies the built DLL to `data/SKSE/Plugins/` for local testing; `build.sh` verifies the cross toolchain and runs the same presets. Remove the copied DLL and `skse/build/` before committing.

## Release packaging

A release contains one manually prepared mod archive. GitHub supplies source archives automatically, so do not create or upload a separate source ZIP.

1. Update the version locations listed above and add the changelog entry.
2. Build with `build-release-msvc`.
3. Verify the generated DLL is copied to `data/SKSE/Plugins/`.
4. Create an archive containing the `data/` directory, preserving the Skyrim layout.
5. Name the archive `OStim-Size-Difference-Manager-vX.Y.Z.zip`.
6. Commit the release as `release: vX.Y.Z`.
7. Create and push the matching annotated tag `vX.Y.Z`.
8. Create a GitHub Release titled `vX.Y.Z` and upload only the mod archive.
9. Remove local build output, the copied DLL, and the local ZIP after publishing.

The install archive should contain the release DLL, INI, and signature manifest under `data/SKSE/Plugins/`. It must not contain source files or build intermediates.

## Documentation

Update the relevant documentation with behavior or compatibility changes:

- `README.md` for player-facing installation, requirements, configuration, and changelog information.
- `docs/ARCHITECTURE.md` for code flow, hook behavior, ABI, and contributor references.
- This document when changing repository, commit, build, or release conventions.

Before submitting a change, run `git diff --check`, verify the intended files with `git status`, and ensure no generated artifacts are staged.
