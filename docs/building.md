# Runtime builds

## Requirements

- Windows with MSVC C++ Build Tools and the Windows SDK.
- CMake 3.25+, Ninja and Python 3.9+ (binary architecture inspection).
- The bundled x86 headers and libraries under `GoldSrc-SDK/external/` and
  `GoldSrc-SDK/lib/`.

Run commands from the repository root in an x86 MSVC developer command prompt,
or initialize the installed Build Tools environment for an x86 target.

## Complete runtime

```powershell
cmake --preset default
cmake --build --preset default --parallel
```

This builds Release with [all 19 production targets](project-scope.md#default-runtime)
and their dependencies. Runtime files go directly into repository-root `output/`,
using the paths expected inside a game installation. Supply game assets and
external providers separately.

Debug uses a separate build and writes to `output-debug/`:

```powershell
cmake --preset runtime-x86-debug
cmake --build --preset runtime-x86-debug --parallel
```

| Preset | Build directory | Runtime output |
| --- | --- | --- |
| `default` / `runtime-x86-release` | `build/release/` | `output/` |
| `runtime-x86-debug` | `build/debug/` | `output-debug/` |
| `hardware-x86-release` | `build/hardware-release/` | `build/hardware-release/output/Release/` |
| `hardware-x86-debug` | `build/hardware-debug/` | `build/hardware-debug/output/Debug/` |

To choose another output root, add
`-DCSE_OUTPUT_DIRECTORY=C:/path/to/game-root` when configuring. Use separate
roots for separate builds. Existing files are not removed automatically.

A build without presets also defaults to Release:

```powershell
cmake -S . -B build/release -G Ninja
cmake --build build/release --parallel
```

## Hardware engine only

```powershell
cmake --preset hardware-x86-release
cmake --build --preset hardware-x86-release --parallel
```

Use `hardware-x86-debug` for Debug. These builds include `hw.dll`, VGUI1,
SDL3 and mpg123. The independent hardware entry point uses the same definitions:

```powershell
cmake -S hardware -B build/hardware-standalone -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/hardware-standalone --parallel
```

## Build options

| Option | Default | Effect |
| --- | --- | --- |
| `CSE_BUILD_DEDICATED` | ON | Build `hlds.exe` and `swds.dll` |
| `CSE_BUILD_BROADCASTING` | ON | Build `hltv.exe` and `proxy.dll` |
| `CSE_BUILD_A3D` | ON | Build `a3dapi.dll`; use requires a compatible legacy provider |
| `CSE_HARDWARE_ONLY` | OFF | Build the hardware engine and its dependencies only |
| `CSE_RELEASE_DIAGNOSTICS` | OFF | Generate Release debug symbols and linker maps |
| `CSE_STAGE_DISTRIBUTION_MATERIALS` | OFF | Copy selected source-provider notices and archives into `licenses/` |
| `CSE_SDL3_FROM_SOURCE` | OFF | Build pinned SDL3 3.4.20 from source instead of the reference x86 binary |

For example, to omit standalone broadcasting and A3D:

```powershell
cmake --preset default -DCSE_BUILD_BROADCASTING=OFF -DCSE_BUILD_A3D=OFF
cmake --build --preset default --parallel
```

Core and DemoPlayer remain enabled when broadcasting is disabled.
`CSE_HARDWARE_ONLY` takes precedence over the other runtime options.

The software client renderer (`sw.dll`) is unavailable; the inherited video
settings still offer it. The dedicated engine (`swds.dll`) is separate.
Windows x64 is in native bring-up. Windows ARM64 remains conditional; complete
macOS has all-target configure presets but incomplete source portability; Linux
composition remains gated. See [migration requirements and status](native-migration.md) before treating an
opt-in target as playable.

## Native Windows bring-up

Initialize the MSVC developer environment for the selected target architecture.
The compiler target is checked independently of the host CPU. Use a separate
build and runtime directory for each architecture and configuration.

```powershell
cmake --preset runtime-windows-x64-release `
  -DCSE_NATIVE_THIRD_PARTY_PREFIX=C:/dependencies/x64-windows-static-md `
  -DCSE_STEAM_PROVIDER_FILE=C:/providers/steam_api64.dll `
  -DCEF_ROOT=C:/providers/cef
cmake --build --preset runtime-windows-x64-release --parallel
```

The native third-party prefix must contain the pinned source-built packages
selected by `cmake/NativeThirdParty.cmake`; the retained x86 archives are not
native link inputs. SDL3 3.4.20 and mpg123 1.33.7 build from the bundled pinned
sources. Steam and CEF artifacts must match the checked-in provider pins and
selected architecture. CEF includes its sandboxed process host, resources and
locales. Architecture checks reject mismatched import libraries and providers.

Use `runtime-windows-x64-debug` with the same explicit provider paths for Debug.
Runtime output is `build/<preset>/runtime/<Configuration>/`. The
`runtime-windows-arm64-release` and `runtime-windows-arm64-debug` presets are
conditional bring-up configurations; they require their own ARM64 dependencies
and complete provider qualification. Merely selecting a preset does not establish
provider availability or runtime support.

The latest frozen Windows x64 cohort builds all 19 targets in Release/Debug.
Selected runtime checks and their limits are in [validation](validation.md).
Rebuild all owned modules together after native ABI changes.

## macOS Apple Silicon bring-up

Use macOS 14+ with Apple Clang, CMake, Ninja and Python. The source-built native
prefix must follow the [pinned dependency recipes](../cmake/dependencies/vcpkg/README.md).
Supply the pinned native Valve library and extracted macOS ARM64 CEF distribution:

```sh
cmake --preset runtime-macos-arm64-release \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX=/absolute/dependencies/cse-arm64-macos \
  -DCSE_STEAM_PROVIDER_FILE=/absolute/providers/libsteam_api.dylib \
  -DCEF_ROOT=/absolute/providers/cef-macosarm64
cmake --build --preset runtime-macos-arm64-release --parallel
```

Use `runtime-macos-arm64-debug` with the same provider paths for Debug. Both
presets select all 19 responsibilities and isolate their runtime under
`build/<preset>/runtime/<Configuration>/`. The native browser helper path is
provided by the preset. No private local provider paths are stored in it.

Both complete graphs configure; complete source compilation/linking is still
being ported. The first full compile records Windows-only platform code and
remaining LP64 boundaries. These presets do not deliver or qualify the complete
macOS game/app bundle yet. Offline Metal shader tools are also required for the
shared renderer. C++ exceptions follow the owning per-file policy; explicitly
annotated Windows Async sources leave fatal native hardware faults to the OS
crash reporter. This does not provide Windows SEH translation or minidump parity.

## Output layout

```text
output/
  hl.exe, hw.dll, hlds.exe, swds.dll, ...
  SDL3.dll
  mpg123.dll
  cstrike/cl_dlls/client.dll
  cstrike/dlls/mp.dll
  valve/cl_dlls/client.dll
  valve/dlls/hl.dll
  valve/cl_dlls/GameUI.dll
  valve/cl_dlls/particleman.dll
```

GameUI and particleman use `valve/cl_dlls/`; Counter-Strike finds them through
its fallback search paths. See the [target list](project-scope.md#default-runtime)
for the full production runtime.

Required import/static libraries and `.exp` files stay under
`build/<build-name>/lib/<Configuration>/<target>/`. Objects and incremental-link
databases stay with the target intermediates. Debug symbols and opt-in Release
diagnostics go under `symbols/<Configuration>/` in the build directory.

Release generates no debug symbols, maps or incremental-link files by default.
Tests, qualification tools and packaging are separate from the runtime build.
Use a fresh output root when disabling an option that previously added files.

Binary distributions need the required notices and source materials described
in [licensing](licensing.md). The distribution-materials option copies the
bundled mpg123 materials; other components retain their own requirements.

## Bundled dependencies

SDL3 3.4.20 headers, import library and DLL are bundled and hash-checked.
Consumers stage `SDL3.dll` in the output root, including when rebuilding an
up-to-date target after the DLL was deleted. See its
[provenance](../GoldSrc-SDK/external/SDL3/UPSTREAM.md).

### Source-built MP3 decoder

The build verifies and extracts the bundled
[mpg123 1.33.7 source archive](../GoldSrc-SDK/external/mpg123/UPSTREAM.md), then
builds its shared decoder without downloading dependencies. MSVC without yasm
uses upstream's generic C decoder. `mpg123.dll` goes beside the engines in the
output root. Music uses mpg123 and SDL3 streams; Windows sound effects use
DirectSound/WaveOut.

## Component builds and tests

Dependency-only presets build pinned SDL3/mpg123 and an ABI probe:

| Platform | Release preset | Debug preset |
| --- | --- | --- |
| Windows x64 | `deps-windows-x64-release` | `deps-windows-x64-debug` |
| Windows ARM64 | `deps-windows-arm64-release` | `deps-windows-arm64-debug` |
| Mac ARM64 | `deps-macos-arm64-release` | `deps-macos-arm64-debug` |

For example, run `cmake --preset deps-macos-arm64-release`, then
`cmake --build --preset deps-macos-arm64-release --parallel`. Each owns its
`build/<preset>/`, stages providers in `runtime/` and keeps the probe in `bin/`.
No provider or game executes during a build.

The analogous `game-modules-<platform>-<cpu>-release` / `-debug` presets build
only HL and CS server modules. Supported combinations are `windows-x64`,
`windows-arm64` and `macos-arm64`. They reuse the generated production sources,
exports and per-file policies, retaining game-relative runtime paths. They do
not build a complete client or engine. Mac uses `hl.dylib` / `mp.dylib`.

[Native fixture READMEs](../tests/native/README.md) provide component commands,
provider inputs and execution boundaries. For the decoder/player/worker tests:

```powershell
cmake -S tests/music -B build/music-tests-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/music-tests-release --parallel
ctest --test-dir build/music-tests-release --output-on-failure
```

Use separate Debug directories. Stream tests use SDL's dummy device; real
Steam, CEF, audio/device and network probes are explicit runtime checks.

## Maintaining generated builds

Update the retained manifests or translator, then regenerate and check:

```sh
python3 tools/translate_msbuild.py
python3 tools/translate_msbuild.py --check
python3 -m unittest discover -s tools/tests
```

[Manifest documentation](msbuild/README.md) describes source membership,
per-file FP/EH/assertion settings, exports/resources and generated inputs. Keep
symbols/import libraries out of runtime outputs and never edit generated target
files independently. [Qualification tools](../tools/qualification/README.md)
cover actual game/server/media runs; [validation](validation.md) records the
pinned cohorts and incomplete gates.

Native-only manifest metadata:

| Property | Meaning |
| --- | --- |
| `CseTargetOS` / `CseReplaces` | Select an OS replacement while preserving the reference source inventory; accepted OS values are `macos`, `linux`, `windows`, `posix` |
| `CseNativeSdl3` | Route non-Windows consumers through the existing pinned SDL3 target; Release/Debug values must agree |
| `CseNativeModuleAliases` | Add guarded relative POSIX library aliases; CS keeps `mp` and provides `cs.dylib` / `cs.so` |

Native game loading uses `gamedll_osx` or `gamedll_linux`. If absent, Windows
`gamedll` entries retain their order and adapt their extension through the VFS.
An alias must not replace an unrelated file.
