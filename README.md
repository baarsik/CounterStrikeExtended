# CounterStrikeExtended

A fork of BlackShadow/GoldSrc-SDK for Counter-Strike 1.6 and Half-Life, including
the engine, game modules, shared runtime, dedicated servers and HLTV. The runtime
uses SDL3 for platform services and libmpg123 for music playback.

The software client renderer is deprecated and planned for removal in a future release.

## AMX Mod X module

The optional `csextended` AMXX module for Win32/Linux32 Counter-Strike servers
provides [extended health and armor](docs/extended-stats.md) and
[server-scoped VGUI2 forms and images](docs/server-forms.md). Pawn plugins can
use both APIs through `#include <csextended>`.

## Building

Supported runtime builds are Windows x86, Windows x64 and macOS Apple Silicon.
All commands run from the repository root and require CMake 3.25+, Ninja and
Python 3.9+. On Windows, use PowerShell with the matching MSVC developer environment.

Initialize the default Counter-Strike server backend before configuring:

```sh
git submodule update --init src/external/regamedll
```

See [server backend selection](docs/building.md#counter-strike-server-backend).

| Platform | Toolchain |
| --- | --- |
| Windows x86 | MSVC C++ Build Tools and Windows SDK, targeting x86 |
| Windows x64 | MSVC C++ Build Tools and Windows SDK, targeting x64 |
| macOS Apple Silicon | macOS 14+ and Apple Clang from Xcode or Command Line Tools |

For Windows x64 and macOS, build the [pinned native dependencies](cmake/dependencies/vcpkg/README.md)
and supply the matching [Steam](tools/qualification/fixtures/steam/lock.json) and
[CEF](tools/qualification/fixtures/cef/lock.json) providers. Replace the example
paths below with absolute paths to those dependencies.

### Windows x86

```powershell
cmake --preset default
cmake --build --preset default --parallel
```

The x86 build uses the bundled dependency headers and libraries.

### Windows x64

```powershell
cmake --preset runtime-windows-x64-release `
  -DCSE_NATIVE_THIRD_PARTY_PREFIX=C:/dependencies/x64-windows-static-md `
  -DCSE_STEAM_PROVIDER_FILE=C:/providers/steam_api64.dll `
  -DCEF_ROOT=C:/providers/cef-windows64
cmake --build --preset runtime-windows-x64-release --parallel
```

### macOS Apple Silicon

```sh
cmake --preset runtime-macos-arm64-release \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX=/absolute/dependencies/cse-arm64-macos \
  -DCSE_STEAM_PROVIDER_FILE=/absolute/providers/libsteam_api.dylib \
  -DCEF_ROOT=/absolute/providers/cef-macosarm64
cmake --build --preset runtime-macos-arm64-release --parallel
```

### Debug builds and output

Use the corresponding Debug preset in both commands, keeping the same provider
paths for native builds:

| Platform | Release preset | Debug preset | Release runtime output |
| --- | --- | --- | --- |
| Windows x86 | `default` | `runtime-x86-debug` | `output/` |
| Windows x64 | `runtime-windows-x64-release` | `runtime-windows-x64-debug` | `build/runtime-windows-x64-release/runtime/Release/` |
| macOS Apple Silicon | `runtime-macos-arm64-release` | `runtime-macos-arm64-debug` | `build/runtime-macos-arm64-release/runtime/Release/` |

Windows x86 Debug writes to `output-debug/`; native Debug builds use
`build/<debug-preset>/runtime/Debug/`. Outputs preserve the `cstrike/` and `valve/`
directory layout. Interface translations are bundled; other game assets are
supplied separately. Static/import libraries,
symbols and intermediates stay in the build directory.

## Documentation

- [Build options, dependencies and output layout](docs/building.md)
- [Runtime targets and source scope](docs/project-scope.md)
- [Maintaining CMake targets and inspecting source inventories](docs/build-system.md)
- [Component tests](tests/native/README.md) and [runtime qualification tools](tools/qualification/README.md)

## Licensing

MIT covers authorized original contributions within the [root license scope](LICENSE).
Inherited Valve/Id code and third-party components retain their own terms. See
[licensing](docs/licensing.md) for details.
