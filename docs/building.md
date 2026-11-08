# Windows x86 build

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
Release flags. The native migration has rebuilt all 19 Win32 targets with Ninja in Release
and Debug and audited their output inventories; see the active migration plan
for evidence scope and remaining reference-fixture work. The [runtime validation report](mpg123-migration-validation.md) covers
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

## Native dependency builds

The migration currently provides separate **dependency-only** builds. These build
SDL3 3.4.20 and mpg123 1.33.7 from pinned local source archives and link a small
provider ABI probe. They do not build a native game or change the Win32 default.
The [migration plan](superpowers/plans/2026-11-08-native64-migration.md) tracks the
remaining runtime, serialization, platform, provider and renderer gates.

On Apple Silicon with Xcode command-line tools:

```sh
cmake --preset deps-macos-arm64-release
cmake --build --preset deps-macos-arm64-release --parallel
cmake --preset deps-macos-arm64-debug
cmake --build --preset deps-macos-arm64-debug --parallel
```

On Windows, initialize MSVC for the selected target architecture, then use
`deps-windows-x64-release` / `deps-windows-x64-debug`, or the conditional
`deps-windows-arm64-release` / `deps-windows-arm64-debug`. A host ARM64 compiler
can emit x64 binaries; the compiler target is checked using its predefined
macros and pointer width. ARM64EC is excluded. An ARM64 target requires the
ARM64 C++ compiler/tools component, in addition to the Windows SDK.

Each preset owns `build/<preset>/`; provider libraries are staged into its
`runtime/`, and the probe stays in `bin/`. Import archives, symbols and build
intermediates remain outside `runtime/`. The macOS minimum is 14.0. No tests,
provider initialization or games run as part of these builds. Execute the probe
only in an authorized fixture; on Windows its `runtime/` must be on `PATH`.

Inspect actual built machine types without loading code:

```sh
python3 tools/binary_identity.py --expect arm64 build/deps-macos-arm64-release/bin/cse_dependency_probe
```

Windows imports are inspected member by member, including short import records.
Directory names cannot establish architecture. Mixed archives, incompatible
pointer widths and unknown machine types fail configuration.

Capture an immutable committed source reference before runtime changes:

```sh
python3 tools/qualification/capture_reference.py --revision <commit> /absolute/path/to/new-reference
python3 tools/qualification/capture_reference.py --verify /absolute/path/to/new-reference
```

This seals committed bytes independently of local edits. Source capture reports
`qualified: false`; it does not replace binary or runtime qualification. After an
all-target Windows build, audit its generated output contract:

```sh
python3 tools/qualification/audit_runtime_inventory.py output --architecture x86 --output /absolute/path/to/new-audit.json
```

The static report includes all generated production outputs, provider binaries,
SHA-256 hashes, exports/ordinals, imports and resource-directory hashes. Unresolved
formal imports and runtime-only provider checks remain explicit. Reports never
claim gameplay or external-server acceptance.

The remaining compression/font/media libraries use the separately pinned
[vcpkg source manifest](../cmake/dependencies/vcpkg/README.md). That document gives
source bootstrap, both-configuration builds, provenance/machine auditing and the
native standalone link fixture. These libraries are not substituted into the
Win32 regression workflow.

`tests/music` now checks the exact decoder PCM against the measured Win32
reference as well as stream and worker behavior. ARM builds select mpg123's
scalar decoder; contraction is disabled in its source build to preserve the
reference rounding. A passing decoder fixture does not qualify the complete
sound-effects mixer, microphone or network voice path.

## Native game module components

During migration, an explicit component build produces the owned Counter-Strike
and Half-Life server libraries. Use a Windows MSVC environment for the selected CPU:

```powershell
cmake --preset game-modules-windows-x64-release
cmake --build --preset game-modules-windows-x64-release --parallel
cmake --preset game-modules-windows-x64-debug
cmake --build --preset game-modules-windows-x64-debug --parallel
```

Replace `x64` with `arm64` for the Windows ARM64 component presets. Each preset
uses its own `build/game-modules-*` directory; DLLs appear below
`runtime/Release` or `runtime/Debug`, retaining `valve/dlls/hl.dll` and
`cstrike/dlls/mp.dll`. Import libraries and symbols stay outside those runtime
directories. On Apple Silicon with Xcode command-line tools, use:

```sh
cmake --preset game-modules-macos-arm64-release
cmake --build --preset game-modules-macos-arm64-release --parallel
cmake --preset game-modules-macos-arm64-debug
cmake --build --preset game-modules-macos-arm64-debug --parallel
```

The macOS outputs use the same game-relative directories with `hl.dylib` and
`mp.dylib`, targeting macOS 14.0. The default Win32 workflow and root output
directories are unchanged.

This entry point reuses the generated production targets, including their
per-file compiler policies. `Sources.cmake` is generated once from the owning
manifests and shared by the MSVC and Clang policy branches. Portable policies
preserve per-file exception handling, FP modes and assertion definitions. Existing
Clang SDK diagnostics remain visible through an explicit nonfatal list; pointer
narrowing remains fatal. Release symbols remain opt-in.

Native Windows definitions preserve the C entry
points and explicit ordinals; C++ `EXPORT` declarations emit native decorated
names. Update owning manifests/source declarations and run the translator when
changing this contract.

These presets build two modules for ABI and data fixtures. They do not build the
client, engine, dedicated launcher or complete runtime, and do not establish
renderer, provider, voice or external-server compatibility. The bounded ABI,
world/team-list and C4 fixtures pass against both game libraries on macOS ARM64
in Release and Debug. Export inventory checks compare every reference C name and
canonical C++ method against the retained Win32 `dumpbin /exports` reports:

```sh
python3 tools/macho_game_exports.py --reference /path/to/Release-hl-win32.txt \
  --library build/game-modules-macos-arm64-release/runtime/Release/valve/dlls/hl.dylib
```

This command checks architecture, minimum macOS version and exports. It does not
load the game or establish gameplay acceptance.


## Native effects fixtures

The native SDL DMA adapter and shared stereo transfer can be tested separately
from the renderer. The maintained Win32 runtime keeps its DirectSound/WaveOut
device; native Windows and POSIX builds select SDL. Mixer/DSP/channel algorithms
remain in the engine. VOX word pointers use native in-memory storage; these words
are reconstructed from sentence text.

```sh
cmake -S tests/native/sound -B build/sound-tests -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCSE_SOUND_SDL_PROVIDER=/absolute/path/to/qualified/libSDL3.0.dylib
cmake --build build/sound-tests --parallel
ctest --test-dir build/sound-tests --output-on-failure
```

Use the architecture-matched source-built provider from the dependency presets.
On Windows also set `CSE_SOUND_SDL_IMPORT_ARCHIVE` to its matching import archive.
The fixture checks actual machine types and pinned headers before linking, then
checks the provider version when explicitly executed. With no provider path,
only the transfer/ring tests build. No provider or game runs during a build.
These fixtures use SDL's dummy device; run them within an authorized fixture.

The reference and native paths must produce identical PCM for clipping, volume,
ring wrapping and high addresses. Provider checks cover exact samples at matching
stream/device rates, write exclusion, focus pause, nested ownership and repeated
shutdown. Hardware-rate conversion, complete native gameplay audio, microphone,
voice and A3D remain separate acceptance work.
