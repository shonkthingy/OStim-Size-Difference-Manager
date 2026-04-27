# OStim Size Difference Manager - TODOs

## 1. UI / SKSE Menu Framework Overhaul
- [x] **Dual Search Bars:** Implement side-by-side search bars (one for Pack Name, one for Animation Name).
- [x] **Hide Empty Packs:** When searching by animation name, hide pack tree-nodes that contain 0 matching results so the user doesn't have to scroll through a massive list of empty folders.
- [x] **Inline Override Inputs:** Remove the "Set Override" popup. Instead, place a number input box directly on the animation row next to the "Override" button so users can quickly type a value and apply it.
- [x] **Pack-Level Mass Actions:** Add "Exempt Pack" and "Override Pack" (with an inline input box) buttons at the pack-folder level to set a new default for all animations inside that folder.
- [x] **Aesthetics & Formatting:** Group options under clearer headings, add spacing, and make the menu look cleaner overall.

## 2. Core Logic Update: Ratio vs. Absolute Difference
- [ ] **Switch from Difference to Ratio:** A flat absolute difference (e.g. `0.2`) treats `Actor 1 (1.0) & Actor 2 (0.8)` the same as `Actor 1 (0.8) & Actor 2 (1.0)`. Changing to a **Size Ratio** (e.g. `0.8` vs `1.25`) allows the mod to differentiate between a "Larger Dom" and a "Larger Sub".
- [ ] **Dom/Sub Role Checking:** Ensure the ratio calculation respects actor slot roles (e.g., usually Actor 1 is the Dom). Prevent scenes authored for a Large Sub from playing on a Large Dom.
- [ ] Update `SceneCache` JSON parsing and runtime matching logic to use this new decimal ratio calculation instead of absolute minus difference.

## 3. Advanced Features & Edge Cases
3+ person animations are untested, and dedicated functionality for them needs to be worked on.

- [ ] **Hub/Transition Node Exemption:** Natively detect and exempt pure transition/hub nodes from scale filtering so the player can always navigate back to previous menu tiers (without relying solely on the `"ostim"` prefix workaround).
- [x] **Soft mode (`Mode=2`):** Closest-match pass when strict finds nothing; see `RandomNodeTwoPass.h`.
- [ ] **OStim scaling compatibility:** Investigate smoother behaviour when users leave OStim's native scaling enabled instead of the **highly recommended** **Disable Scaling** option in Alignment MCM (combining default OStim scaling with this mod's filtering tends to produce janky alignments, but it does not completely break the mod).
- [ ] **Automated Translation/Position Offsets (Low Priority/Hard):** If a 1.0 & 0.8 couple plays an animation authored for 1.2 & 1.0, the ratio is the same (0.8), but the absolute scale means their origin translation alignments will be off. Look into automatically adjusting their Z/Y translation offsets based on the absolute scale delta.

## 4. Maintenance / Architecture
- [ ] **Broader OStim Version Support:** Automate or simplify pattern extraction for new OStim releases (better PDB fallback or dynamic scanning).
- [ ] **Verify Wired INI Flags:** Confirm `ApplyToPlayerScenes`, `ApplyToNpcScenes`, `ApplyInAutoMode`, and `Mode` (Off / Strict / Soft / Debug) are behaving as expected.