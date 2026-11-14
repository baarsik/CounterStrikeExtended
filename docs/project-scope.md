# Project scope

CounterStrikeExtended retains Counter-Strike 1.6, Half-Life, the shared runtime,
dedicated servers and HLTV. Runtime builds target Windows x86, Windows x64 and
macOS Apple Silicon; Windows x86 is the default preset.
The runtime has 19 production CMake targets, enabled by default. They preserve
the runtime scope of 19 of the original 57 non-external MSBuild projects; the
other 38 projects were removed. The active build is maintained directly in
CMake. Broadcasting, A3D and dedicated servers can be disabled for reduced builds.

Source paths below are relative to `src/`. The table shows Windows
runtime paths relative to the selected output root; macOS uses native executable
and library names in the same game directories. See [building.md](building.md)
for commands, output roots and feature options.

## Default runtime

| CMake target | Windows runtime output | Responsibility |
| --- | --- | --- |
| `launcher` | `hl.exe` | Loads the selected engine and filesystem |
| `hw_engine` | `hw.dll` | Hardware engine, media and provider integration |
| `hlds` | `hlds.exe` | Dedicated launcher and administration UI |
| `swds` | `swds.dll` | Dedicated engine; retains shared software-renderer source |
| `cs_client` | `cstrike/cl_dlls/client.dll` | Counter-Strike client and UI |
| `cs_server` | `cstrike/dlls/mp.dll` | Counter-Strike gameplay, bots, hostages, navigation and movement |
| `hl_client` | `valve/cl_dlls/client.dll` | Half-Life client and UI |
| `hl_server` | `valve/dlls/hl.dll` | Half-Life server and shared gameplay |
| `filesystem_stdio` | `filesystem_stdio.dll` | Runtime filesystem |
| `tier0` | `tier0.dll` | Shared runtime support |
| `vgui` | `vgui.dll` | Source-built VGUI1, including its compatibility import library |
| `vgui2` | `vgui2.dll` | Source-built VGUI2 |
| `gameui` | `valve/cl_dlls/GameUI.dll` | Shared game UI |
| `particleman` | `valve/cl_dlls/particleman.dll` | Particle runtime |
| `hltv_core` | `Core.dll` | Demo/world/server interfaces and private support source |
| `demoplayer` | `DemoPlayer.dll` | Engine demo playback |
| `hltv` | `hltv.exe` | Standalone broadcaster |
| `hltv_proxy` | `proxy.dll` | Broadcasting proxy |
| `a3dapi` | `a3dapi.dll` | Legacy A3D adapter |

GameUI and particleman keep their `valve/cl_dlls/` location: Counter-Strike
resolves them through the filesystem's game/fallback search paths. Core and
DemoPlayer remain required with broadcasting disabled. The engine requests
`demoplayer001`; DemoPlayer requests Core's `server001` and `world001` interfaces.

## Reduced runtime options

| CMake option | Targets and outputs | Retained boundary |
| --- | --- | --- |
| `CSE_BUILD_DEDICATED` | `hlds` → `hlds.exe`, `swds` → `swds.dll` | Standalone dedicated launcher and engine; client and listen-server targets remain enabled |
| `CSE_BUILD_BROADCASTING` | `hltv` → `hltv.exe`, `hltv_proxy` → `proxy.dll` | Standalone broadcaster, Proxy and Core's private support/bzip2 source |
| `CSE_BUILD_A3D` | `a3dapi` → `a3dapi.dll` | Legacy adapter, exports/resources and five generated geometry inputs |

All three options default ON. Disable them explicitly for a reduced build. Their
source and CMake target definitions remain available in both Debug and Release.
Building the A3D adapter does not establish compatibility with an A3D provider.

## Removed targets and source

The 38 removed projects comprise:

- Nine client/server pairs: Opposing Force, Blue Shift, Team Fortress Classic,
  Day of Defeat, Deathmatch Classic, Ricochet, Gunman, Condition Zero and
  Condition Zero: Deleted Scenes.
- The software **client** renderer (`sw.dll`) and Worldcraft/Hammer editor.
- Eighteen SDK utilities: bspinfo, light, makefont, makels, mdldec, mdlviewer,
  mkmovie, procinfo, qbsp2, qcsg, qlumpy, qrad, serverctrl, smdlexp, sprgen,
  studiomdl, vis and xwad. Procinfo was a static library and smdlexp a Max plugin;
  neither was a runtime executable.

The complete filenames are in [removed-projects.txt](msbuild/removed-projects.txt).
Their original manifests, exports and source lists remain in the
[immutable upstream archive](msbuild/README.md). No retained target consumes
these outputs or invokes the removed utilities as build-time generators.

Exclusive game/editor/tool implementations and orphan build fragments have been
deleted. Git records their removal; [inherited prose](upstream/README.md)
preserves the original build/tool documents and notices. Condition Zero shared the retained CS source lists;
its target removal did not remove CS gameplay. The original Gunman projects
referenced source absent from this checkout.

The removed `sw.dll` client shared all 187 compile inputs with `swds`, so its
removal did not remove software-renderer source. Inherited launcher and video
settings still contain `sw.dll` selection paths; software client mode has no
built provider. The separate dedicated `swds.dll` remains enabled.

## Shared source that remains required

| Source boundary | Retained consumer or reason |
| --- | --- |
| `particleman/compat/` | Ten unchanged Blue Shift headers preserve particleman's forced/transitive includes, precedence and `BASEMONSTER_H` setting |
| `cstrike/public/game_controls/iclientvgui.h` | Owns the unchanged former DoD interface definition directly, replacing the cross-game forwarding header |
| `utils/common/` | Engine BSP definitions and both games' animation/math headers |
| `utils/sprgen/spritegn.h` | Included through `engine/spritegn.h`; removing the utility target does not remove the format header |
| `utils/vgui/`, `vgui2/` | Runtime UI implementations, controls and VGUI1 import-library generation |
| `HLTV/common/`, Core/DemoPlayer private copies, Console/Proxy | Engine demos, GameUI compilation and optional broadcasting |
| `cstrike/`, `cl_dll/`, `dlls/`, `game_shared/`, `pm_shared/`, `common/`, `public/` | Supported game code and shared interfaces |
| `engine/`, `swds/`, `dedicated/` | Hardware and dedicated engines, media and launcher support |

Shared engine compatibility strings for other games and compiled `engine/cz*`
code are retained. They do not require the removed games' implementation trees.
Required dependencies, resources and compatibility headers remain under
`external/`, `lib/`, `deploy/`, `tracker/` and `network/`. Supported runtime builds
use the CMake presets; obsolete platform Makefiles have been removed.

## Build and provider boundaries

CMake target definitions under `cmake/targets/` own source, flag, library,
resource, export and per-file selections for all 19 production targets. Their
`CMakeLists.txt` and `Sources.cmake` preserve configuration-specific CRT/assertions,
FP/EH overrides, include order, forced headers, exclusions, export names/ordinals
and resources. Inspect effective selections through the
[CMake inventory tool](build-system.md#inspecting-effective-target-settings).
CMake Release diagnostics are intentionally opt-in; Release incremental linking
is disabled. Required intermediates remain under the selected build directory.
The [build-system reference](build-system.md#generated-production-inputs)
describes VGUI1 import-library and A3D generation.
The Windows engine retains its WSOCK32 multicast option values (4/5/6); other
Winsock2 consumers keep their separate API constants. Do not interchange them.

SDL3 is pinned and staged for both engines, both game clients and GameUI.
macOS also links SDL3 into VGUI1, VGUI2, the dedicated launcher and A3D.
Both engines also build/stage the shared mpg123 decoder; music uses SDL3 streams
and no longer requires Miles. Steam, browser, voice and optional legacy modules
also have runtime loading paths; CMake link
dependencies are not the complete module graph. Provider availability in the
qualified SDL3 package does not establish redistribution rights; see
[licensing.md](licensing.md).

The inherited dedicated configuration defines `SW_RENDERER` without explicitly
defining `SWDS_DLL`; shared source has separate `SWDS_DLL` branches. Its export
file declares `LIBRARY sw`, and the target suppresses the resulting linker
warning 4070 while producing `swds.dll`. These settings remain unchanged by
pruning. The dedicated validation does not establish every conditional path.

## Testing

[Component fixtures](../tests/native/README.md) cover individual runtime
boundaries. [Qualification tools](../tools/qualification/README.md) provide
separate game, server, media and provider checks.
