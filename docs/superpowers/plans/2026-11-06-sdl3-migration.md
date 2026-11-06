# Native SDL3 Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the baseline, migrate the retained runtime to native SDL3, preserve behavior and remove SDL2 completely.

**Architecture:** Adapt existing SDL boundaries while retaining engine/client ABI, OpenGL compatibility rendering and native audio. Pin SDL3 and keep ownership explicit across engine, clients and UI. Qualify the same runtime on VM1 before and after migration.

**Tech Stack:** Windows x86, C/C++, MSVC v145, Windows SDK 10.0.26100.0, CMake 4.2+, SDL3 3.4.20, Python/PowerShell qualification tools.

**Spec:** `docs/superpowers/specs/2026-11-06-sdl3-migration-design.md`

## Global Constraints

- Current support remains Windows x86; use VM1 for Windows builds, runtime tests and benchmarks.
- Preserve existing OpenGL compatibility renderer, engine/client ABI, Windows audio and Miles.
- Do not weaken warnings, assertions, FP, ABI or provider requirements.
- Update owning manifests/translator and regenerate CMake/crosswalk; do not hand-edit generated outputs.
- Require native SDL3 and no SDL2 or SDL2-compat runtime dependency.
- Apply repository C++ style to edited code, with four spaces, braces and named domain constants.
- Keep historical original-manifest evidence immutable and distinguish it from active dependencies.

## Review Focus

- Fractional mouse motion and menu/raw-mode transitions must not lose movement or leave stuck input.
- Nanosecond timestamps and pointer-backed UTF-8 events must preserve click thresholds and text.
- Fullscreen/DPI changes must preserve render size, hit testing and native audio window handles.
- Client sign-on, startup media and shutdown must complete before benchmarks count as gameplay evidence.
- Missing providers, indirect imports or inactive source must not silently preserve SDL2 requirements.

### Task 1: Repair and qualify the SDL2 baseline

**Files:** `GoldSrc-SDK/GameUI/LogoFile.cpp`; actual owning startup/network files only if reproduced defects require repair; `tools/qualification/`; baseline evidence documentation.

**Interfaces:** Consume existing runtime presets. Produce a source-built SDL2 baseline, reproducible VM driver and sign-on-qualified benchmark asset/procedure.

- [ ] Reproduce C6336/C2220 and incomplete sign-on with recorded source identity and fresh logs.
- [ ] Fix the GameUI expression ambiguity without changing its evaluation; use build failure/pass as validation.
- [ ] Investigate sign-on and benchmark driver separately; add real regression tests for any behavior defect before fixing it.
- [ ] Build all four presets and verify x86 outputs, providers, actual playable connection and repeated shutdown.
- [ ] Capture baseline behavior/benchmark evidence and commit only the authorized baseline repairs and driver.

### Task 2: Integrate pinned native SDL3 and core window/display events

**Files:** `GoldSrc-SDK/external/SDL3/`, SDL dependency CMake module, owning MSBuild manifests, `tools/translate_msbuild.py`, engine `sys_sdlwind.*`, `sys_getmodes.cpp`, `gl_vidnt.cpp`, `qgl.cpp`, native handle consumers.

**Interfaces:** Preserve `CGame` getters' internal SDL_Window pointer contract and exported engine ABI. Provide native HWND access only at actual Windows consumers. SDL3 includes use `<SDL3/...>`.

- [ ] Add narrow failing tests for event/time/display conversion policies that production will consume.
- [ ] Pin SDL3 3.4.20 with source provenance, licenses, x86 dependency and runtime packaging.
- [ ] Update manifest ownership, bool-result checks, init ownership, native handles, window lifecycle, display IDs, fullscreen modes and physical pixel sizing.
- [ ] Preserve GL compatibility/fallback, extensions, icon byte layout and supported legacy branches; regenerate definitions.
- [ ] Run focused tests and VM1 hardware builds plus window/fullscreen/focus smoke tests; commit.

### Task 3: Migrate client input, VGUI and text/gamepad boundaries

**Files:** engine `VGUI_EngineSurfaceWrap.cpp`, `vgui2/BaseUISurface.*`, `keys.cpp`, `cdll_int.cpp`, VGUI key translation, `GameUI/OptionsSubKeyboard.cpp`, retained game `inputw32.cpp`/`in_camera.cpp` pairs and POSIX clipboard source.

**Interfaces:** Consume SDL3 window/event ownership from Task 2; retain integer engine/client mouse and binding ABI, existing cvar units and old gamepad button meanings.

- [ ] Add failing regression cases for fractional accumulation, 400 ms double-click boundaries, long/multibyte text and gamepad button translation.
- [ ] Port event structures and cursor/relative-mode APIs with explicit precision, timestamp conversion and text lifetime.
- [ ] Port clients together, preserve raw/legacy input paths, binding/layout policies, gamepad enumeration/rescan and clipboard behavior.
- [ ] Run tests and complete runtime builds; compare real menu/game transitions, key/text/controller cases against baseline; commit.

### Task 4: Complete media, retained paths and dependency removal

**Files:** engine `webm_video.*`, `snd_sdl.cpp`, remaining SDL-bearing sources/includes, retained manifests/makefiles, SDL vendor artifacts, obsolete platform VGUI binaries, `tools/qualification/` dependency audit.

**Interfaces:** Consume native SDL3 APIs and preserve startup video, Windows audio and retained non-Windows ring-buffer accounting. Provide an SDL3-only source/build/runtime closure.

- [ ] Add targeted failing checks for any newly factored audio/ring or media conversion policy.
- [ ] Port video paths and retained non-Windows audio; finish all active and retained source consumers.
- [ ] Remove SDL2 headers/import libraries/shared binaries/debug aliases, and rebuild or retire obsolete SDL2-linked platform artifacts with dependency evidence.
- [ ] Regenerate build definitions and audit all current source, manifests and shipped binary dependencies.
- [ ] Run all builds/tests, verify package provider closure and commit.

### Task 5: Qualify behavior, compare benchmarks and review delivery

**Files:** qualification tools/tests, final migration/build/validation documentation and evidence manifest.

**Interfaces:** Consume the qualified baseline and final SDL3 runtime. Produce repeatable acceptance results and zero-SDL2 closure.

- [ ] Run Debug/Release runtime and hardware presets, meaningful automated tests and relevant optional configurations.
- [ ] Exercise CS and HL sign-on/gameplay, dedicated startup, UI/input/media/fullscreen/DPI/audio/focus/shutdown matrix.
- [ ] Run paired fixed-workload benchmarks on VM1; record frame distributions and differences without extrapolating VM results to native hardware.
- [ ] Verify static and live dependencies exclude SDL2 and SDL2-compat, and document all actual limitations.
- [ ] Obtain independent whole-branch review, resolve material findings, run final checks and commit reviewable delivery.
