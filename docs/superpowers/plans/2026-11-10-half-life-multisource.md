# Half-Life Multisource Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the delayed map input that enables Unforeseen Consequences without unsafe array access.

**Architecture:** Keep the existing Half-Life multisource entity and saved fields. Bound its existing lookup and preserve the legacy final-slot fallback for unmatched callers when the input count is valid.

**Tech Stack:** Existing C++ Half-Life server, native Apple Silicon Clang, retained MSVC Win32/x64 policies, actual-source native fixtures.

**Spec:** `docs/superpowers/specs/2026-11-10-half-life-multisource-design.md`.

## Global Constraints

- Production scope is only `GoldSrc-SDK/dlls/buttons.cpp`; focused fixtures belong under `tests/native/multisource/`.
- No class layout, saved field, callback catalog, public ABI, protocol, asset, Counter-Strike game logic, engine, mouse, music, renderer, permission or microphone change. The microphone consent issue remains explicitly deferred.
- Preserve accepted CS110 binaries and all saves. Controller alone owns live applications, VM1 and root integration. Never operate VM2.
- One source implementer at a time. Use the existing clean isolated macos-dedicated worktree at `2a7b2f41be0185b1cf5a7427988372af7529219e`; do not import prerequisite snapshot commits into root.
- Reuse retained owning compiler/link policy, at most two concurrent compile commands. Do not suppress new production warnings or broaden a build without a concrete dependency need.
- Keep old and new build/source packets immutable and separate. Report exact commands, exits, complete output and SHA256 pins; no physical acceptance claim from fixtures.

## Review Focus

1. The chapter's one-input delayed relay must enable its master after the delay; a warning-only change cannot resolve the blockage.
2. First/middle/last member matches must retain their precise slot, while valid unmatched/null input uses the final slot.
3. Counts0, negative and above32 must leave arrays and target dispatch untouched; full32-input lookup must stay in bounds.
4. Initialization, repeated toggles, global-state gating and output USE_TOGGLE/USE_ON semantics must remain consistent with the existing actual entity bodies.
5. Native pointer width must not change saved fields/layout or callbacks, and new server images must preserve exports and the working engine/client/provider tuple.

### Task 1: Restore bounded legacy multisource behavior

**Files:** Modify `GoldSrc-SDK/dlls/buttons.cpp`, only `CMultiSource::Use`. Create `tests/native/multisource/CMakeLists.txt`, `extract.py`, `multisource_tests.cpp` and `README.md` as needed; do not add unused scaffolding.

**Interfaces:** Preserve `void CMultiSource::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)`, existing `BOOL CMultiSource::IsTriggered(CBaseEntity *)`, class fields, save descriptors and callback catalog. No new public or private production interface is needed.

- [ ] Step 1: Inspect the complete actual Use/IsTriggered/Register bodies and delayed-caller path. Build an actual-source fixture with named entity-handle and engine target seams. Freeze the current production source before editing; demonstrate RED for one registered relay, zero triggered bit, DelayedUse caller, expecting that bit1 and IsTriggered true. Report exact compile/run commands and diagnostic output.
- [ ] Step 2: Add explicit behavioral assertions: valid counts1,2,32; first/middle/last match; unmatched and null caller selecting the final registered slot; count0,-1,33 with unchanged arrays/canaries and no dispatch; repeated on/off toggle; initializing master; global state off/on; output USE_TOGGLE without global state and USE_ON with enabled global state. Use complete actual decision bodies, not a duplicated state-machine implementation. Bind the chapter regression to the preserved map entity values without copying game assets into git.
- [ ] Step 3: Change only the existing Use body to validate the count before lookup/access and preserve the legacy final-slot result for unmatched valid input. Use MS_MAX_TARGETS and existing types; apply root C++ style within the changed function. Add a short reason for the compatibility fallback. Do not add fields or alter delayed entities, IsTriggered, saved descriptors or Counter-Strike code.
- [ ] Step 4: Run focused Release, Debug and Debug ASan/UBSan tests. Compile the actual native Half-Life buttons.cpp owner and full-link corresponding Release/Debug server dylibs into new immutable packets under the existing owning policy. Record source/dependency/object closure and unchanged inputs, symbols/exports/imports and final images. Identify the precise Windows owning-file compilation needs for the controller. If an admission/cohort constraint requires another production change, report it before editing beyond the approved scope.
- [ ] Step 5: Self-review, freeze source, test logs and images, commit task-only paths and yield source ownership. Append full report with original-source RED, covering GREEN, exact argv/output, hashes and limitations. Controller independently reviews before root integration/staging; controller's physical gate observes natural chapter teleport and later save/transition behavior with the current hardware renderer and audio unchanged.
