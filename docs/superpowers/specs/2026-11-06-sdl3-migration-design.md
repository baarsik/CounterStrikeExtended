# Native SDL3 migration

The owner approved the direct native SDL3 approach from the assessment and authorized
baseline repairs, complete behavior preservation, SDL2 removal, and all required VM1
builds, runtime tests and benchmarks. This document records that authorized scope.

## Intended result

The current Windows x86 Counter-Strike, Half-Life, dedicated-server and optional
runtime builds use SDL3 directly. Existing OpenGL compatibility rendering, engine and
client interfaces, native Windows audio, Miles, game logic and networking remain.
SDL2 headers, libraries, runtime binaries and compatibility layers are absent from
active source/build/runtime dependencies. Retained inactive source must also be
migrated. Obsolete platform binaries with SDL2 load dependencies must be rebuilt or
removed from the retained deliverable; original historical manifests remain immutable
evidence, clearly separated from current build definitions.

## Approach

Use pinned SDL3 3.4.20 with native calls. An SDL2 compatibility runtime would retain
the dependency the owner wants removed. Replacing rendering or audio backends would
expand behavior risk beyond the migration's purpose. All supported source clients
migrate with the engine; externally supplied SDL2-importing modules require rebuilds.

Fix the baseline first: the GameUI C6336 build failure, any actual startup/sign-on
defects, and shortcomings in the benchmark driver. Keep genuine runtime failures
distinct from harness timing mistakes. Capture an SDL2 baseline before migration.

## Required behavior

- Preserve OpenGL compatibility context creation/fallback, extension loading, FBO,
  MSAA, vsync, gamma tables, desktop/exclusive fullscreen, refresh selection and DPI.
- Preserve focus/minimize/restore/quit lifecycle, raw and legacy mouse paths,
  acceleration/filtering, capture/warp ordering, integer client ABI and fractional
  motion accumulation, buttons and wheels.
- Preserve physical key bindings, layout-sensitive labels, UTF-8 text, clipboard,
  double-click thresholds, gamepad mapping/rescan policy and UI cursor transitions.
- Preserve startup media, DirectSound/WaveOut fallback, music/voice, non-Windows
  retained audio ring/DMA semantics, and clean repeated shutdown.
- Keep generated CMake and source crosswalk synchronized through their translator.

## Evidence and release gates

Use VM1 only for Windows build/runtime qualification. Run all four checked-in
Debug/Release hardware/runtime presets with MSVC v145 and SDK 10.0.26100.0. Do not
weaken warnings, assertions, FP, ABI or provider requirements. Add meaningful tests
at changed arithmetic/event boundaries and run real integration paths. Require
complete client sign-on before recording gameplay measurements; partial sign-on
renderer timings are insufficient. Compare fixed assets/settings and report frame
distributions and limits of VM measurements. Inspect x86 PE imports and live module
closure, and require SDL2/SDL2-compat absence. Keep a behavioral acceptance matrix;
unverified cases must remain visible and must not be called preserved by inference.

## Constraints and decisions

Current support remains Windows x86. Native macOS/Linux/x64/ARM64 implementations
are separate projects. All VM1 actions necessary for this work are authorized.
Use an isolated branch and preserve the original checkout. No publishing, merging
or third-party messaging is part of this request.
