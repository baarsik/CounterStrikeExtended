# Windows x86 build

The baseline retains Counter-Strike 1.6, Half-Life, dedicated server and shared
runtime source. Builds produce real runtime modules from production source;
they do not invoke MSBuild as implementation wrappers or copy runtime DLLs in
place of targets. No test or launch target is defined.

## Requirements

- Windows, Visual Studio 2026 Desktop development with C++, **MSVC v145** and
  Windows SDK **10.0.26100.0**.
- **CMake 4.2 or later** (the first version with the
  [Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)).
- The supplied x86 headers, import/static libraries and configuration variants
  under `GoldSrc-SDK/external/` and `GoldSrc-SDK/lib/`. Missing libraries fail
  configuration; x64/ARM64 and macOS/Linux runtime builds are rejected.

No suitable Windows builder was available in the authoring environment.
**These source/build changes are not yet Windows-compiled or runtime-qualified.**
Do not infer playability from CMake target availability.

## Complete runtime

From the repository root, on the Windows builder:

```powershell
cmake --preset runtime-x86-release
cmake --build --preset runtime-x86-release --parallel
cmake --preset runtime-x86-debug
cmake --build --preset runtime-x86-debug --parallel
```

Both presets include dedicated-server support. Their 16 targets are `launcher`,
`hw_engine`, `hlds`, `swds`, `cs_client`, `cs_server`, `hl_client`, `hl_server`,
`filesystem_stdio`, `tier0`, `vgui`, `vgui2`, `gameui`, `particleman`, `hltv_core`,
and `demoplayer`. Game client/server target names are distinct even when output
filenames match. The aggregate target is `cse_runtime`.

## Hardware-only entry point (I3)

```powershell
cmake --preset hardware-x86-release
cmake --build --preset hardware-x86-release --parallel
cmake --preset hardware-x86-debug
cmake --build --preset hardware-x86-debug --parallel
```

This configures only `hw_engine` and its source-built VGUI1 dependency. The
aggregate target is `cse_hardware`. An independent small source entry point is
also supplied:

```powershell
cmake -S hardware -B build/hardware-standalone -G "Visual Studio 18 2026" -A Win32 -T v145 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build build/hardware-standalone --config Release --target cse_hardware --parallel
```

It uses the same target definitions and settings rather than a second engine
implementation. It does not require game clients, servers, SDK utilities or
optional broadcaster projects to build.

## Optional features and unavailable modes

`CSE_BUILD_BROADCASTING=ON` adds `hltv` (`hltv.exe`) and `hltv_proxy`
(`proxy.dll`). `CSE_BUILD_A3D=ON` adds `a3dapi`. Both default OFF, as approved;
their source is preserved. For example:

```powershell
cmake --preset runtime-x86-release -DCSE_BUILD_BROADCASTING=ON -DCSE_BUILD_A3D=ON
cmake --build --preset runtime-x86-release --parallel
```

Core and DemoPlayer remain enabled with broadcasting OFF. A3D still depends on
a legacy provider; building its adapter does not prove operational A3D support.
`CSE_BUILD_DEDICATED` defaults ON and can be disabled for a reduced configuration.
The complete-runtime presets explicitly enable it. `CSE_HARDWARE_ONLY` takes
precedence over all broader runtime options.

The approved software client renderer and other-game/tool targets are removed.
The inherited launcher/video-settings source still contains `sw.dll` selection
paths: selecting software client mode will find no built `sw.dll`. The dedicated
`swds.dll` output is separate and remains available. No launcher redesign is
included in the narrow corrections.

## Output separation and providers

Each preset uses its own `build/<preset>/` directory:

```text
runtime/Debug/ or runtime/Release/     EXEs and DLLs only
  cstrike/cl_dlls/client.dll          Counter-Strike client
  cstrike/dlls/mp.dll                 Counter-Strike server
  valve/cl_dlls/client.dll            Half-Life client
  valve/dlls/hl.dll                   Half-Life server
  valve/cl_dlls/GameUI.dll
  valve/cl_dlls/particleman.dll
lib/<Configuration>/<target>/        import/static libraries and .exp files
symbols/<Configuration>/<target>/    PDBs (engine maps at symbols/<Configuration>/)
targets/                            CMake/compile intermediates
```

The root runtime directory contains launchers, engines, filesystem, UI libraries,
Core and DemoPlayer. VGUI1's import archive is rebuilt from its names-only
`vgui_imports.def` after the source-built DLL, matching the upstream dependency
step. Same-basename objects are separated by CMake's source paths; import libraries
and linker/compile PDBs have target-specific directories even when runtime
basenames match. Executable exports are explicitly modeled as well.

There is no install into a live Steam directory and no automatic provider
loading. Game data and compatible `mss32.dll`, `SDL2.dll`, `steam_api.dll`,
Chrome/voice/optional providers and their redistribution rights are separate
requirements. Their absence is not hidden by stubs or treated as a successful
runtime package. Miles source, linkage, worker and command paths are preserved.

## Source crosswalk and Windows evaluation

See [the per-target crosswalk](msbuild-crosswalk.md) and its exact configuration/
per-file metadata in `msbuild/repository-selections.json`. The checked-in CMake
target modules can be regenerated without a compiler:

```powershell
python tools/translate_msbuild.py
```

This expands the repository's `.props`/`.items` and configuration conditions;
it is not a Microsoft toolchain-default evaluator and is not invoked by CMake
when building runtime code. Original manifests remain in the archive documented
in `msbuild/README.md`; 19 retained/optional manifests also remain live.

On a suitable Windows builder, collect actual MSBuild-evaluated item/property
metadata and preprocessed imports **without building targets**:

```powershell
powershell -File tools/export-msbuild.ps1
```

The command uses Microsoft's
[`-getProperty`/`-getItem` evaluation interface](https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties)
without a target or target-result option. Compare item metadata against the
repository crosswalk, inspect Link item definitions in the expanded XML, then
compile both configurations and inspect PE architecture, exports/ordinals,
imports and embedded resources. This evaluation has not been run here.

The MSVC defaults hook removes CMake's initial generic Release `NDEBUG` and
Debug instrumentation defaults; each target instead receives its own manifest
settings. Explicit user cache flag overrides remain the user's responsibility.
Per-file flags are scoped to individual target directories and override the
relevant inherited option without adding conflicting target-level FP/EH flags.
No global fast-math or ICF policy is introduced. Whole-program optimization,
static/DLL CRT variants, assertion definitions, includes and forced headers are
translated per target/configuration.

## A3D generated production inputs

The optional A3D project has 16 ordinary compile inputs plus five generated
geometry translation units. Its upstream `PrepareGeometryCompilation` target
selects OBJECTS, FINALIZE, ACOUSTICS, SUBMISSION and STATE sections of production
`A3dGeom.cpp`, in addition to the ordinary CORE input. CMake generates unique
wrapper sources in the a3dapi target's build directory, with the original section
selectors and individual FP/optimization/intrinsic/instruction-set/EH settings.
The wrappers compile production code, not stand-ins for a missing provider.
The repository crosswalk includes these five target-time inputs explicitly;
MSBuild's evaluation-only `-getItem` output alone does not include additions made
inside a build target. Inspect the preserved target body in the preprocessed XML.
