# Windows x86 build

The build retains Counter-Strike 1.6, Half-Life, dedicated server and shared
runtime source. Tests and runtime qualification are separate from production builds.

## Requirements

- Windows, Visual Studio 2026 Desktop development with C++, **MSVC v145** and
  Windows SDK **10.0.26100.0**.
- **CMake 4.2 or later** (the first version with the
  [Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)).
- The supplied x86 headers, import/static libraries and configuration variants
  under `GoldSrc-SDK/external/` and `GoldSrc-SDK/lib/`. Missing libraries fail
  configuration; x64/ARM64 and macOS/Linux runtime builds are rejected.

The [current validation report](mpg123-migration-validation.md) records VM1
build/runtime results after source cleanup and the music migration. The earlier
[SDL3 report](sdl3-migration-validation.md) contains the SDL2/SDL3 comparison.

## Complete runtime

From the repository root, on the Windows builder:

```powershell
cmake --preset runtime-x86-release
cmake --build --preset runtime-x86-release --parallel
cmake --preset runtime-x86-debug
cmake --build --preset runtime-x86-debug --parallel
```

Both presets include the [16 default runtime targets](project-scope.md#default-runtime)
and their dependencies. The aggregate target is `cse_runtime`.

## Hardware-only entry point

```powershell
cmake --preset hardware-x86-release
cmake --build --preset hardware-x86-release --parallel
cmake --preset hardware-x86-debug
cmake --build --preset hardware-x86-debug --parallel
```

This configures `hw_engine` with source-built VGUI1 and mpg123 dependencies. The
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
(`proxy.dll`). `CSE_BUILD_A3D=ON` adds `a3dapi`. Both default OFF;
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

The software client renderer and other-game/tool targets are removed.
The inherited launcher/video-settings source still contains `sw.dll` selection
paths: selecting software client mode will find no built `sw.dll`. The dedicated
`swds.dll` output is separate and remains available.

## Output separation and providers

Each preset uses its own `build/<preset>/` directory:

```text
runtime/Debug/ or runtime/Release/    runtime modules and dependency materials
  cstrike/cl_dlls/client.dll         Counter-Strike client
  cstrike/dlls/mp.dll                Counter-Strike server
  valve/cl_dlls/client.dll           Half-Life client
  valve/dlls/hl.dll                  Half-Life server
  valve/cl_dlls/GameUI.dll
  valve/cl_dlls/particleman.dll
  licenses/mpg123/                   decoder source and notices
lib/<Configuration>/<target>/       import/static libraries and .exp files
symbols/<Configuration>/<target>/    PDBs (engine maps at symbols/<Configuration>/)
targets/                            CMake/compile intermediates
```

The root runtime directory contains launchers, engines, filesystem, UI libraries,
Core and DemoPlayer. VGUI1's import archive is rebuilt from its names-only
`vgui_imports.def` after the source-built DLL, matching the upstream dependency
step. Same-basename objects are separated by CMake's source paths; import libraries
and linker/compile PDBs have target-specific directories even when runtime
basenames match. Executable exports are explicitly modeled as well.

Pinned SDL3 3.4.20 headers, x86 import library and DLL are bundled; consumers stage
`SDL3.dll` in the runtime root in both configurations. Both engines also build
and stage the [shared mpg123 decoder](#source-built-mp3-decoder). Music uses SDL3
streams and no longer requires Miles.

Game data, compatible Steam/browser/voice/optional providers and their
[redistribution rights](licensing.md) remain separate requirements. Build outputs
stay in the selected build directory; no live Steam installation is modified.

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
inspect PE architecture, exports/ordinals, imports and embedded resources from
both configurations. The repository crosswalk is a source evaluator; the SDL3
build/runtime results do not establish exhaustive Microsoft-evaluated parity.

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

## SDL3 provider and retained platform recipes

`GoldSrc-SDK/external/SDL3/UPSTREAM.md` records the official package and selected
unmodified payload hashes. `python tools/sdl3_dependency.py` validates this pin;
configuration/translation also fail when a pinned input is missing or modified.
No installed SDL fallback, SDL2 provider or compatibility runtime is selected.
MSBuild `SDL3.props`/`SDL3.targets` and the generated CMake dependency model stage
the same x86 DLL even when building an individual consumer that is up to date.

The retained Linux/macOS client Makefiles require native `pkg-config sdl3` and an
explicit `VGUI_LIBRARY=/absolute/path/to/rebuilt/library`. They fail if either
provider is absent. SDL2-linked `linux/vgui.so` and `linux/release/vgui.dylib` were
retired; `linux/release/vgui.so` remains because its formal dependencies contain
no SDL2. Its presence does not qualify it as the required rebuilt provider.
These Carbon/i386 recipes do not establish a supported native Linux/macOS port.
The retained platform audio stream preserves the 64 KiB engine DMA ring;
Windows sound effects still use DirectSound/WaveOut.

For an SDL3-only test package, use the explicit provider/data fixture and formal
import audit in [qualification utilities](../tools/qualification/README.md).
Raw Steam-data overlays can contain SDL2 and unused stock engine alternatives;
copying source-built modules over that data is insufficient closure evidence.

## Source-built MP3 decoder

The [pinned mpg123 1.33.7 source](../GoldSrc-SDK/external/mpg123/UPSTREAM.md) is
bundled as an unmodified archive. The CMake wrapper verifies its SHA-256, extracts
it into the build tree and builds upstream's shared decoder without a network
download. MSVC without yasm uses upstream's generic C decoder.

CMake stages `mpg123.dll` beside `hw.dll` and `swds.dll`. Import libraries and
symbols are under `lib/<Configuration>/mpg123/` and `symbols/<Configuration>/mpg123/`.
Standalone MSBuild imports `Mpg123.props` and `Mpg123.targets`; each engine builds
its own isolated dependency beneath its IntDir and stages to the shared runtime root.
Both routes stage COPYING, AUTHORS, provenance and the complete source archive
under `licenses/mpg123/` beside the runtime DLLs.

Run real decoder, stream lifecycle and CD worker tests on the Windows builder:

```powershell
cmake -S tests/music -B build/music-tests -G "Visual Studio 18 2026" -A Win32 -T v145 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build build/music-tests --config Release --parallel
ctest --test-dir build/music-tests -C Release --output-on-failure
cmake --build build/music-tests --config Debug --parallel
ctest --test-dir build/music-tests -C Debug --output-on-failure
```

Stream tests use SDL's dummy device. Actual game-output captures and their limits
are recorded in [VM1 validation](mpg123-migration-validation.md).
