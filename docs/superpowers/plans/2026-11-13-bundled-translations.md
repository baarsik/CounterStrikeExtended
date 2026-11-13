# Bundled Translations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Bundle maintained stock CS/HL and CSE/admin translations for all eight official languages in supported builds.

**Architecture:** Keep reviewable UTF-8 source files under `src/deploy/localization`, preserving stock runtime paths. A deterministic Python generator emits compatible UTF-16 token files and stages them through a shared CMake dependency. Existing loaders retain English fallback; CSE additions get their own catalogue.

**Tech Stack:** Python 3.9+, CMake 3.25+, existing C++ VGUI localization and filesystem services.

**Spec:** `docs/superpowers/specs/2026-11-13-bundled-translations-design.md`

## Global Constraints

- Languages: `english`, `french`, `german`, `italian`, `spanish`, `schinese`, `tchinese`, `koreana`; preserve the legacy `korean` alias.
- Maintain Valve translations as the stock baseline; preserve provenance and report upstream fallback gaps.
- Include stock UI, legacy interface text, CSE additions and administration text.
- Ordinary builds are offline and independent of a Steam installation.
- Cover Windows x86/x64 and macOS ARM64, Release/Debug, custom runtime roots, and restoration without relinking.
- Do not bundle non-text game assets, change Steam preferences or add a language picker.
- Apply repository C++ conventions and keep changes within this task.

## Review Focus

- Missing staged files after an otherwise current build: staging restores them.
- Selected-language resources in a game fallback directory: translated text wins over English.
- Quotes, backslashes, CJK text and substitution arguments: generation preserves their runtime meaning.
- Korean API spelling versus legacy filenames: both resolve to the same language.
- Incomplete official catalogues: coverage distinguishes inherited gaps from translated CSE entries.

### Task 1: Import and document the stock catalogue

**Files:** Create `src/deploy/localization/manifest.json`, `src/deploy/localization/README.md`, and UTF-8 text files at mirrored runtime-relative paths.

**Interfaces:** The manifest exposes `languages`, `files` (source, destination, encoding, provenance/hash), and baseline coverage. Later tasks consume only checked-in files and this manifest.

- [x] Inventory installed official resources and obtain missing official language content into an isolated acquisition cache. Pin hashes and source identities.
- [x] Import complete `valve`, `cstrike`, `gameui`, `platform`, `vgui`, `serverbrowser` and administration families, preserving upstream text and attribution.
- [x] Inventory `titles.txt`, menu and key-binding labels; import applicable official localized text and record fallback behavior.
- [x] Check every required language/family and record upstream gaps explicitly. Do not mark this task complete while baseline acquisition is unresolved.
- [x] Commit the import and provenance.

### Task 2: Validate, generate and stage translation files

**Files:** Create `tools/build_localization.py`, `cmake/Localization.cmake`, `tests/localization_resources/test_resources.py`, `tests/localization_resources/CMakeLists.txt`; modify `cmake/RuntimeTargets.cmake`, `cmake/query/CMakeLists.txt` and affected target definitions.

**Interfaces:** `python3 tools/build_localization.py --source SOURCE --output OUTPUT` validates the manifest, emits runtime files and copies only changed bytes. `cse_localization_assets(target)` attaches shared generation/staging and exposes source/destination metadata to inventory tools.

- [x] Write failing tests for Unicode/escape/placeholder preservation, invalid or missing manifest input, Korean aliases, incremental updates and restoration of a deleted staged file.
- [x] Run `python3 -m unittest discover -s tests/localization_resources -v`; confirm expected failures before implementation.
- [x] Implement the generator using the standard library; output UTF-16LE BOM for VGUI catalogues and the declared encoding for legacy text.
- [x] Add shared CMake ownership and attach relevant runtime consumers. Replace duplicate admin translation staging and update its resource inventory check.
- [x] Run the Python suite and CMake staging fixture for Debug/Release/custom output roots; inspect Windows/macOS inventory selections. Expected: complete identical destination sets and restoration without relinking.
- [x] Commit generator, CMake and tests.

### Task 3: Translate and load CSE/admin additions

**Files:** Add `platform/resource/cse_<language>.txt` source catalogues and any missing administration language sources; modify `src/GameUI/native/ServerBrowserDialog.cpp` and the existing affected UI test integration.

**Interfaces:** Existing `ILocalize::AddFile` loads `resource/cse_%language%.txt` before browser strings are consumed. Resource tokens remain `CSEServerBrowser_*`; missing-file fallbacks derive from the maintained English catalogue if existing contracts require them.

- [x] Add a failing real-loader test that resolves translated CSE text in a Western language and CJK languages, plus English fallback and existing custom-token preservation.
- [x] Author translations for all fourteen current CSE browser tokens and required admin entries in all eight languages.
- [x] Load the CSE catalogue at the correct point and remove independently maintained English duplication, preserving emergency behavior where necessary.
- [x] Run the resource suite, actual localization loader checks and affected browser/admin tests; compile changed production C++ with its owning recipe.
- [x] Commit runtime loading and translations.

### Task 4: Verify the complete bundled interface and document maintenance

**Files:** Modify `docs/building.md`, `docs/licensing.md`, `tests/native/README.md` and catalogue maintenance documentation as appropriate.

**Interfaces:** Existing configure/build presets bundle translations by default. Documentation gives catalogue ownership, regeneration/staging behavior, source provenance and exact verification limits.

- [x] Verify all manifest destinations, token coverage and preserved substitution arguments against the approved scope, including legacy HUD paths.
- [x] Run supported host build targets and affected existing suites; inspect all supported platform/configuration resource inventories.
- [x] Exercise representative translated UI rendering if the local runtime is usable; record any unavailable runtime checks precisely.
- [x] Update maintenance/build/provenance documentation and run `git diff --check`.
- [x] Obtain a fresh whole-branch review, resolve material findings, and rerun affected checks.
- [x] Commit final fixes/documentation and deliver the branch with evidence and any remaining limitations.

## Execution outcome

Implemented on `codex/bundled-translations`. See the
[verification record](../validation/2026-11-13-bundled-translations.md) for observed
checks, fixed review findings and explicit runtime limits. The existing browser
`unsaved` case fails identically with the baseline implementation; rendered UI and
whole-game acceptance remain unqualified.
