# Build manifest reference

## Regenerating CMake targets

`tools/translate_msbuild.py` expands the retained `.props`, `.items` and project
conditions for Debug/Release Win32. Run it from the repository root after changing
manifests or the translator:

```powershell
python tools/translate_msbuild.py
```

It writes the target modules under `cmake/targets/`, the
[per-target crosswalk](../msbuild-crosswalk.md) and exact selections in
`repository-selections.json`. CMake builds use these checked-in definitions;
translation is a separate maintenance step.

The translator evaluates repository inputs. It does not evaluate Microsoft
toolchain defaults or user properties.

## Configuration policy

Each target has its own source/flag scope. Translation preserves CRT selection,
assertion definitions, include order, forced headers, exclusions, resources,
exports and per-file floating-point/exception settings. The MSVC defaults hook
clears generic CMake configuration flags before target settings are applied.
Explicit cache flag overrides remain the caller's responsibility.

CMake gates Release debug information and maps behind `CSE_RELEASE_DIAGNOSTICS`
and disables Release incremental linking. The crosswalk records inherited
manifest settings before that output policy is applied.

## Generated production inputs

VGUI1 rebuilds its compatibility import archive from the names-only
`utils/vgui/vgui_imports.def` after linking `vgui.dll`.

A3D has 16 ordinary compile inputs and five generated geometry translation units.
The upstream `PrepareGeometryCompilation` target selects OBJECTS, FINALIZE,
ACOUSTICS, SUBMISSION and STATE sections of `A3dGeom.cpp`, alongside CORE.
CMake creates unique wrappers in the target build directory and applies the
original per-file settings. These generated inputs appear in the crosswalk;
evaluation-only MSBuild item output does not include inputs added inside a target.

## MSBuild evaluation

On a Windows builder, collect evaluated properties/items and preprocessed imports
without compiling targets:

```powershell
powershell -File tools/export-msbuild.ps1
```

This uses MSBuild's `-getProperty` / `-getItem` interface. Compare the output and
expanded Link settings against the repository crosswalk. Inspect the preserved
A3D target body for its generated inputs. PE architecture, exports, imports and
resources require separate checks of built binaries.

The retained SDL3 dependency imports validate and stage the pinned x86 DLL.
`Mpg123.props` and `Mpg123.targets` build an isolated decoder under each engine's
intermediate directory and stage its runtime, notices and source archive.

## Retained platform recipes

The Linux/macOS client Makefiles require native `pkg-config sdl3` and an explicit
`VGUI_LIBRARY=/absolute/path/to/rebuilt/library`. Their Carbon/i386 paths do not
establish a supported native port. The retained platform audio stream preserves
the 64 KiB engine DMA ring. See the [native migration requirements](../native-migration.md)
for platform and provider limitations.

## Archived originals

`upstream-manifests.zip` preserves 166 original non-external project, property,
item, solution, export, resource and filter manifests from upstream HEAD
`c10c1100792ce1c23f7941ff350f4c4746151ee1`. Entries retain their original bytes
and repository-relative paths, including references to removed games and SDL2.
They are historical comparison evidence, not current build instructions.

SHA-256: `fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2`

The 38 removed projects are listed in [removed-projects.txt](removed-projects.txt).
The 19 retained projects and their imports remain live under
`GoldSrc-SDK/`; their current selections are in the
[generated crosswalk](../msbuild-crosswalk.md). Archive preservation does not
establish Microsoft toolchain/user-property evaluation. Extract into a separate
copy of the original tree for comparison. Removed source remains recoverable
from the upstream Git commit.

All inherited notices and rights apply unchanged to the archive contents. This archive is not covered by the license for original build contributions.
