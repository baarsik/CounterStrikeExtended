# Project scope

CounterStrikeExtended retains Counter-Strike 1.6, Half-Life, the shared runtime
and dedicated-server support. The supported build configuration is Windows x86.
Of the original 57 non-external MSBuild projects, 16 are enabled by default,
3 are optional, and 38 have been removed.

Paths below are relative to `GoldSrc-SDK/`, except runtime outputs, which are
relative to `build/<preset>/runtime/<Configuration>/` for CMake builds. The
legacy MSBuild projects use `GoldSrc-SDK/bin/Win32/<Configuration>/` instead.
See [building.md](building.md) for commands, output separation and feature options.

## Default runtime

| CMake target | Runtime output | Responsibility |
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

GameUI and particleman keep their `valve/cl_dlls/` location: Counter-Strike
resolves them through the filesystem's game/fallback search paths. Core and
DemoPlayer remain required with broadcasting disabled. The engine requests
`demoplayer001`; DemoPlayer requests Core's `server001` and `world001` interfaces.

## Optional runtime

| CMake option | Targets and outputs | Retained boundary |
| --- | --- | --- |
| `CSE_BUILD_BROADCASTING` | `hltv` → `hltv.exe`, `hltv_proxy` → `proxy.dll` | Standalone broadcaster, Proxy and Core's private support/bzip2 source |
| `CSE_BUILD_A3D` | `a3dapi` → `a3dapi.dll` | Legacy adapter, exports/resources and five generated geometry inputs |

Both options default OFF. Their source and live MSBuild projects remain; the
legacy solution still has Debug/Release `Build.0` entries for all three projects.
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
deleted. [Source pruning](source-pruning.md) records the stages, relocated
headers and archived prose. Condition Zero shared the retained CS source lists;
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
Dependency/resource directories such as `external/`, `lib/`, `deploy/`,
`tracker/` and `network/` are outside the game-tree cleanup. Retained platform
recipes do not establish a supported Linux/macOS port.

## Build and provider boundaries

The [generated crosswalk](msbuild-crosswalk.md) and
`msbuild/repository-selections.json` are the authoritative repository source,
flag, library, resource, export and per-file selections for all 19 projects.
Generation preserves configuration-specific CRT/assertions, FP/EH overrides,
include order, forced headers, exclusions, export names/ordinals and resources.
[Building](building.md) describes the VGUI1 import-library and A3D generation
steps; [source corrections](source-corrections.md) records C1–C4.

Native SDL3 is pinned and staged for the five linker consumers: both engines,
both game clients and GameUI. Both engines also build/stage the shared mpg123
decoder; music uses SDL3 streams and no longer requires Miles. Steam, browser,
voice and optional legacy modules also have runtime loading paths; static
project references are not the complete module graph. Provider availability in
the qualified SDL3 package does not establish redistribution rights; see
[licensing.md](licensing.md).

The inherited dedicated configuration defines `SW_RENDERER` without explicitly
defining `SWDS_DLL`; shared source has separate `SWDS_DLL` branches. Its export
file declares `LIBRARY sw`, and the project suppresses the resulting linker
warning 4070 while producing `swds.dll`. These settings remain unchanged by
pruning. The dedicated results below do not establish every conditional path.

## Verification scope

[SDL3 migration validation](sdl3-migration-validation.md) records Windows x86
builds, exercised runtime paths and coverage limits for its pinned snapshot.
The subsequent residual-game cleanup has source-selection, header-closure and
generation checks recorded in [source-pruning.md](source-pruning.md). The current
post-cleanup runtime with libmpg123 is covered separately by
[music migration validation](mpg123-migration-validation.md).
