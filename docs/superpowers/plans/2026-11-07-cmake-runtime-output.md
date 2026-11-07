# CMake Runtime Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Build all retained Windows x86 runtime targets with CMake/Ninja and produce a clean game-relative Release output.

**Architecture:** Keep generated target modules and their manifest translator. Centralize output locations in the runtime helpers and gate inherited Release diagnostics during generation. Use Ninja presets with a configured x86 MSVC command-line environment.

**Tech Stack:** CMake 3.25+, Ninja, MSVC Build Tools, Windows SDK, Python unittest.

**Spec:** `docs/superpowers/specs/2026-11-07-cmake-runtime-output-design.md`

## Global Constraints

- Windows x86 and existing MSVC ABI remain required; no IDE, VS2026, v145 or exact SDK dependency.
- Default Release builds all 19 retained runtime targets into repository-root `output/`.
- Debug output is separate. Non-runtime artifacts need explicit opt-in.
- Required build intermediates stay in the build tree; preserve dependency source and notices in the repository.
- Synchronize generated CMake modules through `tools/translate_msbuild.py`.

## Review Focus

- Same-named game DLLs must retain separate game-relative destinations.
- An up-to-date consumer must restore a deleted dependency DLL.
- Release diagnostic switches must cover target and per-file flags and the decoder.
- Standalone hardware and test entry points must keep isolated output directories.
- Explicit Debug and Release builds must not overwrite each other's output.

### Task 1: CMake entry points and runtime layout

**Files:** `CMakeLists.txt`, `CMakePresets.json`, `hardware/CMakeLists.txt`, `cmake/RuntimeTargets.cmake`, dependency modules, `.gitignore`, `tools/tests/test_runtime_build.py`.

**Interfaces:** `CSE_OUTPUT_DIRECTORY` selects the game root; `CSE_RELEASE_DIAGNOSTICS` and `CSE_STAGE_DISTRIBUTION_MATERIALS` default OFF.

- [x] Add executable CMake fixture checks for game-relative output, private import libraries, staging restoration and opt-in materials; observe the missing output behavior.
- [x] Implement shared locations, all-target defaults, Ninja presets and CMake 3.25 floor without MSVC/SDK version pins.
- [x] Run `python3 -m unittest discover -s tools/tests -v`; expect all checks green after Task 2.

### Task 2: Minimal Release artifacts

**Files:** `tools/translate_msbuild.py`, generated `cmake/targets/*/CMakeLists.txt`, decoder wrapper, `tools/tests/test_runtime_build.py`.

**Interfaces:** Generated diagnostic predicates consume Task 1's `CSE_RELEASE_DIAGNOSTICS` option.

- [x] Add real CMake evaluation checks for inherited Release diagnostic flags, OFF and ON, plus Debug preservation; observe failing default behavior.
- [x] Gate Release compiler/linker debug information and maps, disable incremental Release linking, and regenerate target modules.
- [x] Run build/translation tests and regenerate twice; expect green tests and identical generated files.

### Task 3: Documentation and validation

**Files:** `AGENTS.md`, `README.md`, `docs/building.md`, `docs/project-scope.md`, `docs/licensing.md`.

**Interfaces:** Document the implemented presets and options, game-root output and independent qualification paths.

- [x] Update current build instructions and requirements without rewriting historical evidence.
- [x] Run Python build and qualification suites, CMake preset listing and diff checks; expect no failures.
- [x] If VM1 command transport works, build full Release and Debug using Ninja and inspect runtime contents and PE debug directories. Otherwise record the exact Windows validation limitation.
- [x] Request an independent whole-change review and address important findings before delivery.
