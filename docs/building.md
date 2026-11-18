# Build reference

See the [README](../README.md#building) for complete Release build commands for
Windows x86, Windows x64 and macOS Apple Silicon. Run commands from the repository
root with CMake 3.25+, Ninja and Python 3.9+.

## Platforms and dependencies

Windows builds require MSVC C++ Build Tools and the Windows SDK. Initialize a
developer shell for the target architecture; the build checks the
compiler target independently of the host CPU. macOS builds require macOS 14+
on Apple Silicon and Apple Clang from Xcode or Command Line Tools.

Windows x86 uses the bundled headers and libraries under `src/external/`
and `src/lib/`. Windows x64 and macOS use source-built packages from the
[pinned dependency recipes](../cmake/dependencies/vcpkg/README.md), plus external
Steam and CEF providers:

| CMake variable | Required input |
| --- | --- |
| `CSE_NATIVE_THIRD_PARTY_PREFIX` | Absolute path to the architecture-specific install prefix, containing both Release and Debug libraries |
| `CSE_STEAM_PROVIDER_FILE` | Absolute path to `steam_api64.dll` on Windows x64 or `libsteam_api.dylib` on macOS, matching the [Steam pin](../tools/qualification/fixtures/steam/lock.json) |
| `CEF_ROOT` | Absolute path to the extracted Windows x64 or macOS ARM64 distribution matching the [CEF pin](../tools/qualification/fixtures/cef/lock.json) |

The build verifies provider hashes and binary architecture. CEF helpers,
resources and locales are staged with the runtime. Rebuild all project modules
together after native ABI changes; native processes require matching native
modules and providers.

Full Windows ARM64 and Linux runtime builds are unavailable. Windows ARM64
component presets are listed under [component builds](#component-builds-and-tests).
Windows x64 and macOS have partial gameplay, device and provider validation.
On macOS, an initial microphone permission prompt can delay startup until answered.
The software client renderer is deprecated and planned for removal in a future
release. On Windows x64 and macOS, `-soft` selects the bundled software renderer
in `swds.dll` or `swds.dylib`, respectively. On Windows x86, `-soft` expects a
separate `sw.dll`.

## macOS graphics backend

The macOS ARM64 hardware engine uses Metal by default. **Options → Video → Renderer**
shows the active backend and lets you choose **Metal** or **OpenGL**. Apply restarts the
game using the selected renderer and saves that choice for later launches. Other video
changes preserve the active backend. Metal is listed only in macOS builds that include it.

`-metal` selects Metal explicitly; `-gl` selects OpenGL. These launch flags override the
saved menu choice. Supplying both flags is an error.
Metal initialization requires SDL_GPU's `metal` driver; failure is reported rather than
silently creating an OpenGL context. Windows x86/x64 continue to use their existing renderer.

`CSE_ENABLE_METAL` defaults to `ON` on macOS and `OFF` elsewhere. Metal builds compile
and embed shaders offline; install Apple's Metal toolchain and pass its compiler and linker
when configuring, together with the existing provider arguments:

```sh
cmake --preset runtime-macos-arm64-release \
  -DCSE_METAL_COMPILER="$(xcrun --toolchain Metal --find metal)" \
  -DCSE_METAL_LINKER="$(xcrun --toolchain Metal --find metallib)"
cmake --build --preset runtime-macos-arm64-release --target cse_client --parallel
```

The same arguments apply to the Debug preset. Missing offline tools are a configuration
error for an enabled build. `-DCSE_ENABLE_METAL=OFF` produces an OpenGL-only client without
requiring Metal tools; an explicit `-metal` request is rejected by that build. GPU sources
and shader tools belong to the hardware engine, not dedicated-server or HLTV targets.
Metadata-only CMake inventory queries do not require the shader toolchain.

The native path includes fixed-function world/studio drawing, palettes, screenshots,
startup video, advanced radar and Original/Smoked UI. Default scene sampling is 4x MSAA;
`-nomsaa` selects one sample. See the [Metal renderer qualification fixtures](../tests/native/metal_renderer/README.md)
for reproducible checks, pixel tolerances and measured limits. These checks do not establish
an FPS improvement across games or machines.

## Presets and outputs

| Release preset | Debug preset | Release build directory | Release runtime output |
| --- | --- | --- | --- |
| `default` / `runtime-x86-release` | `runtime-x86-debug` | `build/release/` | `output/` |
| `runtime-windows-x64-release` | `runtime-windows-x64-debug` | `build/runtime-windows-x64-release/` | `build/runtime-windows-x64-release/runtime/Release/` |
| `runtime-macos-arm64-release` | `runtime-macos-arm64-debug` | `build/runtime-macos-arm64-release/` | `build/runtime-macos-arm64-release/runtime/Release/` |
| `hardware-x86-release` | `hardware-x86-debug` | `build/hardware-release/` | `build/hardware-release/output/Release/` |

Windows x86 Debug uses `build/debug/` and `output-debug/`. Native Debug builds
use `build/<debug-preset>/runtime/Debug/`; hardware-only Debug uses
`build/hardware-debug/output/Debug/`.

To choose another runtime root, add `-DCSE_OUTPUT_DIRECTORY=/absolute/game-root`
when configuring. Keep separate roots for each architecture and configuration.
Existing output files are not removed automatically, including files from
previously enabled options.

The default build includes [all 19 production targets](project-scope.md#default-runtime)
and their dependencies. `cse_runtime` explicitly selects the complete runtime;
`cse_client` selects the 14 client and listen-server targets plus their dependencies.
Stock and CSE interface translations and ZBot resources are bundled. Other game
assets are supplied separately. Builds do not launch games or run tests.

## Counter-Strike bots

`CSE_ENABLE_ZBOT` defaults to `ON` for runtime and game-module builds on every
supported platform. It enables ZBot commands, cvars, navigation and the
Counter-Strike create-server controls without requiring the Condition Zero game.
Set `-DCSE_ENABLE_ZBOT=OFF` when configuring to disable those features and omit
their resource staging. Shared bot/navigation code remains compiled for existing
game DLL references. As with other build options, files from a previous enabled
build are not deleted automatically.

Building `cs_server` or `gameui` stages the bot profiles, chatter database, all
referenced English voice recordings, create-server layouts and a `de_dust2` nav
mesh beneath the runtime's `cstrike/` directory. Missing resources are restored
on subsequent builds without relinking. Other maps use ZBot's existing navigation
learning; the game still requires its separately supplied maps and other assets.
See the [bot resource notes](../src/deploy/zbot/README.md) for provenance and map
compatibility. Use `bot_add`, `bot_add_ct`, `bot_add_t` or the create-server bot
controls to add players; the server's initial `bot_quota` remains zero.

## Interface translations

Runtime builds bundle English, French, German, Italian, Spanish (Spain), Simplified
Chinese, Traditional Chinese and Korean text, plus CSE-maintained Russian and
Polish translations. The shared resource target covers
Windows x86/x64 and macOS ARM64 in Release and Debug, including custom runtime
roots. It restores missing resource files on subsequent builds without relinking
consumers and leaves unchanged files untouched.

Maintained UTF-8 catalogues live under `src/deploy/localization/`; CMake generates
the runtime encodings and the browser's emergency English fallback include in the
build tree. Stock translation gaps retain English fallback. See the
[catalogue maintenance and provenance notes](../src/deploy/localization/README.md)
for the language manifest, source hashes and update procedure.

Choose **Options → Interface → Language** to save a bundled language; applying a
change updates menus and HUD in the running CS or Half-Life session. Use
`-language russian` or `-language polish` when starting the client or
dedicated GUI to select these CSE-maintained translations independently of Steam.
Language identifiers are normalized and validated before forming resource paths.

## Hardware engine only

From the repository root in an x86 MSVC developer shell:

```powershell
cmake --preset hardware-x86-release
cmake --build --preset hardware-x86-release --parallel
```

Use `hardware-x86-debug` for Debug. These builds include `hw.dll`, VGUI1,
SDL3 and mpg123.

## Build options

| Option | Default | Effect |
| --- | --- | --- |
| `CSE_BUILD_DEDICATED` | ON | Build the dedicated launcher and engine |
| `CSE_BUILD_BROADCASTING` | ON | Build the standalone HLTV broadcaster and proxy |
| `CSE_BUILD_A3D` | ON | Build the A3D adapter |
| `CSE_HARDWARE_ONLY` | OFF | Build the hardware engine and its dependencies only |
| `CSE_RELEASE_DIAGNOSTICS` | OFF | Generate Release debug symbols and linker maps |
| `CSE_STAGE_DISTRIBUTION_MATERIALS` | OFF | Copy selected provider notices and source archives into `licenses/` |
| `CSE_SDL3_FROM_SOURCE` | OFF for x86; ON in native runtime presets | Build pinned SDL3 from source instead of using the bundled x86 binary |

For example, to omit standalone broadcasting and A3D:

```powershell
cmake --preset default -DCSE_BUILD_BROADCASTING=OFF -DCSE_BUILD_A3D=OFF
cmake --build --preset default --parallel
```

Core and DemoPlayer remain enabled when broadcasting is disabled.
`CSE_HARDWARE_ONLY` takes precedence over the other runtime options.
The Windows legacy A3D path requires a compatible external provider.

## Runtime layout

Windows x86 Release uses the following layout; Windows x64 preserves the same
relative paths beneath its runtime root:

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

macOS preserves the game directories with native executables and `.dylib`
modules. GameUI and particleman stay under `valve/cl_dlls/`; Counter-Strike
resolves them through its fallback search paths.

Required import/static libraries and `.exp` files stay under
`build/<build-name>/lib/<Configuration>/<target>/`. Objects and incremental-link
databases stay with target intermediates. Debug symbols and opt-in Release
diagnostics go under `symbols/<Configuration>/` in the build directory.
Release produces no debug symbols, maps or incremental-link files by default.

Binary distributions require the notices and source materials described in
[licensing](licensing.md). The distribution-materials option copies bundled
mpg123 and libwebp materials; other components retain their own requirements.

## Bundled media dependencies

SDL3 3.4.20 headers, import library and DLL are bundled and hash-checked for x86.
Native runtime presets build the pinned source instead. Consumers stage the
shared library in the runtime root. See [SDL3 provenance](../src/external/SDL3/UPSTREAM.md).

### Source-built MP3 decoder

The build verifies and extracts the bundled
[mpg123 1.33.7 source archive](../src/external/mpg123/UPSTREAM.md), then
builds its shared decoder without downloading dependencies. MSVC without yasm
uses upstream's generic C decoder. The shared library is staged beside the
engines. Music uses mpg123 and SDL3 streams; Windows sound effects use
DirectSound/WaveOut.

## Component builds and tests

Dependency-only presets build pinned SDL3/mpg123 and an ABI probe:

| Platform | Release preset | Debug preset |
| --- | --- | --- |
| Windows x64 | `deps-windows-x64-release` | `deps-windows-x64-debug` |
| Windows ARM64 | `deps-windows-arm64-release` | `deps-windows-arm64-debug` |
| macOS ARM64 | `deps-macos-arm64-release` | `deps-macos-arm64-debug` |

For example:

```sh
cmake --preset deps-macos-arm64-release
cmake --build --preset deps-macos-arm64-release --parallel
```

Each preset owns `build/<preset>/`, stages shared libraries in `runtime/` and
keeps the probe in `bin/`.

The `game-modules-<platform>-<cpu>-release` / `-debug` presets build only the
Half-Life and Counter-Strike server modules. Available combinations are
`windows-x64`, `windows-arm64` and `macos-arm64`. They reuse production sources,
exports and per-file policies, retaining game-relative runtime paths.

[Native fixture READMEs](../tests/native/README.md) provide component commands,
provider inputs and coverage. For decoder, player and worker tests:

```sh
cmake -S tests/music -B build/music-tests-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/music-tests-release --parallel
ctest --test-dir build/music-tests-release --output-on-failure
```

Use separate Debug directories. Stream tests use SDL's dummy device;
[qualification tools](../tools/qualification/README.md) cover game, server,
provider and device checks separately.

## Maintaining CMake builds

CMake is the build source of truth. Edit `cmake/targets/<target>/CMakeLists.txt`
and its adjacent `Sources.cmake` directly; shared runtime helpers live in
`cmake/RuntimeTargets.cmake`. The [build-system reference](build-system.md)
describes source membership, configuration and platform conditions, per-file
FP/EH/assertion settings, exports, resources, generated inputs and CMake-derived
inventory inspection.

Native game loading uses `gamedll_osx` or `gamedll_linux`. If absent, Windows
`gamedll` entries retain their order and adapt their extension through the VFS.
An alias must not replace an unrelated file.

## Studio model batching

The hardware renderer batches each studio mesh into one indexed triangle draw by
default. `r_studio_batch 0` selects the retained immediate renderer for A/B checks;
`r_studio_batch 1` enables batching again. Animation, lighting and material settings
keep their existing behavior. Metal submits the indexed geometry through its typed native
recorder. Diagnostic capture modes and unsupported client GL state use the immediate path automatically.

The [studio batching fixtures](../tests/native/studio_batch/README.md) cover topology,
engine attribute integration, client state and optional real OpenGL pixel comparisons.
Their model-topology microbenchmarks do not measure gameplay FPS.

## VGUI styles and reusable background blur

**Options → Interface → UI style** selects **Original** or **Smoked Tactical Glass** (the default).
Apply/OK changes the style without restarting. The archived `vgui_style` values remain `0` for Original
and `1` for Smoked Tactical Glass, so existing configurations retain their selection.

Smoked Tactical Glass is shared by both VGUI2 control implementations and embedded VGUI1. Outer windows
and popup menus use rounded charcoal frost; inner forms/lists use denser surfaces, off-white text and
amber selection/focus. Controls and artwork paint afterward and remain sharp. Counter-Strike spectator
controls share a compact rail, primitive chevrons and a separate health readout; the passive bars retain
matching darkness/blur fades. Custom CS viewport, scoreboard, tutorial and career dialog shells opt
into the same material; the legacy HL scoreboard does too. Original uses the retained scheme, layout
and drawing paths.

Smoked uses capped text and spacing metrics, flat tabs and aligned Interface fields with keyboard-accessible
radio choices backed by the same archived style value. Options keeps one window size across all tabs,
bounded by the workspace. Resource geometry uses the shared control density and restores its Original
scale on a style change; window chrome shares consistent insets and icon sizes. Fixed captions use fitting fonts. Menus share padded text,
check and cascade columns; empty rows become noninteractive block separators. Long menus scroll within
the screen. Display-only dropdowns paint a padded label without editor selection; editable dropdowns keep
normal text editing. Symbol fonts, fixed-width legacy text, semantic team/status colors and content artwork
retain their owners.

`src/public/vgui_smoked_glass.h` owns the palette, material roles and rounded geometry. VGUI2 controls
bind the optional `CSEVGuiStyle001` service from `src/public/vgui_style_service.h`; its draw operation uses
absolute screen bounds/parent clipping rather than a VGUI2 handle. Embedded VGUI1 binds the same service
through the additive `CSE_VGui1BindStyle` entry point and converts its inverse-alpha convention. Existing
VGUI interfaces, class layouts and virtual slots are unchanged. Standalone tools without the engine
service retain their existing presentation. Missing blur uses rounded dark fills.

Live style changes reload schemes children-before-parent, including hidden descendants, preserving
parent-owned icons and child transparency without replacing panels or clearing input. Explicit VGUI1
RGB colors retain player/team, killer and mute-hover meaning. Other semantic colors remain producer-owned.

The existing `CSEVGuiBackgroundEffects001` API and `vgui2::BackdropPanel` remain available for independent
gradient backgrounds. The hardware engine captures the world before HUD drawing, resolves MSAA,
downsamples to quarter dimensions and applies a separable Gaussian filter. All panels share this GPU image;
they do not capture or reblur foreground UI. Capture is demand-driven: a newly visible panel uses its
fallback on the first frame. Original and frames without requests perform no blur filtering. Resize
recreates the targets and context shutdown releases them.

The standalone real-OpenGL fixture covers pixels, clipping, matching gradients, foreground ordering,
read/draw framebuffer restoration, MSAA, demand and resize:

```sh
cmake -S tests/native/vgui_backdrop -B build/vgui-backdrop -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_BACKDROP_SDL_LIBRARY=/absolute/path/to/the/native/SDL3/shared-library
cmake --build build/vgui-backdrop --parallel
ctest --test-dir build/vgui-backdrop --output-on-failure
```

## Image formats

Native image paths accept static lossy or lossless WebP, including transparency.
For legacy `.tga` names and extensionless UI names, a same-basename `.webp` is
preferred when available; existing TGA and BMP fallback remains supported.
Explicit `.webp` paths are accepted. Existing game assets need no conversion.
Scope images honor the server's exact-file requirements: an unchecked local WebP
cannot replace a checked TGA. Cached images refresh after server resources arrive.

The `screenshot` and `snapshot` commands, numbered movie frame captures and
`envmap` cubemap captures write lossless `.webp` images. Capture dimensions,
colors and existing naming/overwrite rules are retained.
`src/devtools/image_to_background.py` also generates lossless WebP tiles and
matching background layout names.

libwebp 1.6.0 is built statically from the checked-in, SHA-256-verified archive
under `src/external/libwebp-source/`; no system WebP installation or additional
runtime DLL is required. The owning modules select a provider matching their
MSVC CRT. Static archives and upstream intermediates stay in the build directory.
For redistribution, include the source archive, `COPYING` and `PATENTS` from
that directory.

The focused codec tests run independently of the game:

```sh
cmake -S tests/native/webp_codec -B build/webp-codec -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/webp-codec --parallel
ctest --test-dir build/webp-codec --output-on-failure
```

## Counter-Strike radar

The native client includes the NextClient advanced radar, enabled by default.
Choose **Options → Interface → HUD Radar…** for mode, map source, scale,
opacity, rotation, player labels/health/trails and full-map scoreboard settings.
The dialog stages edits until Apply or OK; Cancel discards pending edits.
`hud_radar_mode 0` selects the classic radar. The classic radar also takes over
while an advanced map source is unavailable.

Generated textured and flat maps select floors automatically; overview mode
uses the map's supplied overview. Map corrections are staged with the CS client
under `cstrike/overviews/`. The port is Counter-Strike-specific; Half-Life's
shared Interface tab disables this settings entry.

Radar shader sources live under `src/cstrike/cl_dll/nextclient/shaders/` and are
embedded by CMake in the selected build directory. See the
[port provenance](../src/cstrike/cl_dll/nextclient/UPSTREAM.md).
