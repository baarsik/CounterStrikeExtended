# Interface Options Implementation Plan

> **For agentic workers:** Use independent persistence/startup and page authoring tasks with root integration, then one fresh whole-change review.

**Goal:** Add a usable, localized Interface → Language dropdown after Video.
**Architecture:** Existing PropertyPage and native settings, generated manifest language table, startup resolver precedence CLI > saved > Steam/English.
**Tech Stack:** C++17 GameUI, existing C++ helpers/native settings, Python generator, CMake.
**Spec:** docs/superpowers/specs/2026-11-13-interface-options-design.md

## Constraints and review focus

- Ten bundled languages plus automatic; immutable identifiers, non-editable combo.
- Apply/OK persists, Cancel/reset does not; failed writes remain retryable.
- No immediate restart or Steam preference mutation; explicit launch override stays effective.
- Saved values must be bounded/validated and read before filesystem/surface startup.
- Interface follows Video and all tabs/labels/notes fit minimum game geometry in all languages.
- Preserve existing custom safe `-language` support, Korean aliases and offline staging.

### Task1: Generated table and persistence/startup

**Files:** generator/manifest, common language helpers, native preference reader, owning CMake targets, selector fixtures.
**Interfaces:** generated cse::interface_language::BundledLanguages array with id/nativeName/englishName; ReadSavedPreference returns a bounded id or empty; Resolve(current,requested,saved) retains two-argument compatibility. ISystem preference key constant is shared.

- [x] Add tests for generated table parity, saved/CLI precedence and invalid values; observe RED.
- [x] Generate the table and stage dependencies; implement native startup reads and matching dedicated resolution.
- [x] Verify automatic/invalid/failed/missing state, unchanged custom override and owning compilation.

### Task2: Page and localized UI

**Files:** OptionsSubInterface.cpp/h, OptionsDialog.cpp, owning Sources.cmake, ten CSE catalogues and native UI fixture.
**Interfaces:** COptionsSubInterface(parent), OnResetData/OnApplyChanges; non-editable combo records stable language id in item data, uses shared preference key and generated table. Page owns restart/save/override status labels; uses ISystem and current surface language.

- [x] Add real page behavior and ordering tests first; observe missing-feature failures.
- [x] Add the page immediately after Video, localize all captions/notices and adapt tab strip/page geometry.
- [x] Validate Apply, Cancel/reset, unchanged selection, failed write/retry, launch-option notice and all ten list entries.
- [x] Measure/render actual Options/page controls at640x480 for all languages; preserve keyboard access and tabs.
- [x] Regenerate sources, run affected suites/builds, obtain fresh review and fix material findings; commit and document evidence/limits.

## Verification

The final code review has no open material findings. Actual UI/control, native
preference and owning-build evidence is summarized in
[the validation record](../validation/2026-11-13-interface-options.md).
The branch remains local; no merge or publication is part of this change.
