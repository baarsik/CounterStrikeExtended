# Windows x86 build

## Requirements

- Windows with MSVC C++ Build Tools and the Windows SDK.
- CMake 3.25+ and Ninja.
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
| `CSE_STAGE_DISTRIBUTION_MATERIALS` | OFF | Copy mpg123 notices and source into `licenses/mpg123/` in the output root |

For example, to omit standalone broadcasting and A3D:

```powershell
cmake --preset default -DCSE_BUILD_BROADCASTING=OFF -DCSE_BUILD_A3D=OFF
cmake --build --preset default --parallel
```

Core and DemoPlayer remain enabled when broadcasting is disabled.
`CSE_HARDWARE_ONLY` takes precedence over the other runtime options.

The software client renderer (`sw.dll`) is unavailable; the inherited video
settings still offer it. The dedicated engine (`swds.dll`) is separate.
Windows x64/ARM64 and macOS/Linux runtime builds are unsupported.

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

## Build maintenance and validation

Update retained manifests or `tools/translate_msbuild.py`, then regenerate the
checked-in target definitions:

```powershell
python tools/translate_msbuild.py
```

The [build manifest reference](msbuild/README.md) explains translation,
configuration settings, generated inputs and archived evidence.

Local checks cover Ninja presets, output paths, dependency restoration and
Release flags. Full Windows Ninja compilation and EXE/DLL inspection remain
pending. The [runtime validation report](mpg123-migration-validation.md) covers
earlier builds, gameplay and music; [qualification tools](../tools/qualification/README.md)
describe the separate runtime checks.

Run the decoder, stream and CD worker tests in the Windows build environment:

```powershell
cmake -S tests/music -B build/music-tests-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/music-tests-release --parallel
ctest --test-dir build/music-tests-release --output-on-failure
```

For Debug, use a separate build directory with `-DCMAKE_BUILD_TYPE=Debug`.
Stream tests use SDL's dummy audio device.
