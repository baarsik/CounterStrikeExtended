# Project scope and proposed pruning

Date: 2026-11-06. Status: **Approved by owner on 2026-11-06; source implementation recorded below.**

Inspected source HEAD: `c10c1100792ce1c23f7941ff350f4c4746151ee1`.
The checkout was clean before this document was added. Work is on
`codex/project-scope-inventory`. No project, source, build setting, or license has
been changed by this inventory.

## Approved decision

The owner approved the disposition of all **57 non-external `.vcxproj` files** and the staged source boundaries, and pre-approved the remaining implementation phases:

| Disposition | Projects | Proposed treatment |
| --- | ---: | --- |
| Keep | 16 | Counter-Strike, Half-Life, launchers, dedicated server, shared UI/filesystem/demo runtime |
| Optional | 3 | Preserve HLTV broadcasting and A3D projects/source; make their future CMake participation explicit |
| Remove | 38 | 18 other-game targets, the software client renderer, Hammer, and 18 SDK utility targets |
| Unresolved project disposition | 0 | Dependency and build questions below remain unresolved independently of target scope |

The approval covers these 38 target removals and the staged source
pruning boundaries below. It does **not** authorize deleting entire game or
utility trees based on their names. Original MSBuild manifests, imported source
lists, export definitions, resources, and notices remain available until the
retained CMake translation has been audited against them. Removing a target from
the active build and deleting its historical manifest are separate operations.

Counter-Strike 1.6 **and Half-Life** remain in scope. So do `hlds.exe`, `swds.dll`,
Miles music, Core, and DemoPlayer. The initial build target is Windows x86;
project configuration alone does not establish x64 or ARM64 support.

## Evidence and limits

All SDK paths below are relative to `GoldSrc-SDK/`. Runtime output paths are
relative to `GoldSrc-SDK/bin/Win32/<Configuration>/`, unless stated otherwise.
Every project declares `Debug|Win32` and `Release|Win32`.

This was a source and dependency inventory on macOS, using:

- `git status --porcelain=v1`, `git log -1 --format='%H%n%P%n%s'`, and
  `rg --files` to establish the baseline and enumerate manifests.
- Python standard-library XML inspection of all 57 projects, both repository
  `.props` files, and all four `.items` files. Repository imports were followed
  in order; Debug/Release conditions, target properties, source exclusions,
  per-file metadata, libraries, `.def`/`.rc` inputs, and build events were read.
- A conservative recursive include scan from retained and optional compile
  items, listed headers, and forced includes, using the repository include
  paths. This deliberately includes inactive preprocessor branches and listed
  headers: an edge is a reason to retain a file, not proof it is compiled in
  every configuration. Direct include edges below were also inspected manually.
- `rg` inspection of runtime module loading, preprocessor guards, source paths,
  and linker pragmas; solution `Build.0` mappings were read separately.
- Read-only parsing of COFF archives for bundled public/Miles libraries.

Compile-entry counts below exclude explicit `ExcludedFromBuild=true` entries;
the resulting file sets are the same for Debug and Release at this HEAD. They
are manifest counts, not preprocessed function counts or build results. All
active retained/optional C/C++ paths exist, accounting for Windows path casing.
The Gunman projects reference missing source, recorded below.

This is **not an MSBuild-evaluated CMake crosswalk**. Microsoft C++ default
props/targets and machine-local user props were not loaded, and property
functions were inspected from their source expressions. In particular, the A3D
`NormalizeDirectory` expressions and CS's expression removing inherited
`NDEBUG` must be preserved in the later evaluation. No `msbuild` is on this
host's PATH. No Windows builder was used or borrowed from another chat.

**No tests, CTest, configuration, compilation, games, DLL loading, runtime
probes, providers, or network/audio workers were executed.**

## Keep: 16 runtime projects

| Project | Runtime output | C/C++ entries | Required source/dependency boundary |
| --- | --- | ---: | --- |
| `launcher/launcher.vcxproj` | `hl.exe` | 4 | Launcher source, `hl.def`, `hl.rc`; loads engine and filesystem |
| `projects/vs2026/hw_engine.vcxproj` | `hw.dll` | 173 | `engine/`, VGUI controls, common containers; project reference to VGUI1; Miles/SDL/Steam/codecs |
| `projects/vs2026/hlds.vcxproj` | `hlds.exe` | 54 | `dedicated/`, engine/common/VGUI control source and admin resources; loads `swds.dll` |
| `swds/swds.vcxproj` | `swds.dll` | 187 | Engine software-renderer source, VGUI controls, common containers; VGUI1 reference; same retained media/provider families as hardware |
| `projects/vs2026/cstrike_cdll.vcxproj` | `cstrike/cl_dlls/client.dll` | 168 | `CounterStrike.props`, `cstrike-client.items`, CS private/shared/UI source, one shared VGUI control source; SDL/Steam |
| `projects/vs2026/cstrikedll.vcxproj` | `cstrike/dlls/mp.dll` | 136 | `CounterStrike.props`, `cstrike-server.items`; CS gameplay, bots, hostages, navigation, movement and private headers |
| `projects/vs2026/hl_cdll.vcxproj` | `valve/cl_dlls/client.dll` | 83 | `cl_dll/`, `dlls/`, `game_shared/`, `pm_shared/`, `common/`, `public/`; VGUI1 reference, SDL and `game_controls.lib` |
| `projects/vs2026/hldll.vcxproj` | `valve/dlls/hl.dll` | 102 | `dlls/`, shared voice and movement source; `dlls/hl.def` |
| `filesystem/filesystem_stdio.vcxproj` | `filesystem_stdio.dll` | 6 | Filesystem source and `public/interface.cpp`; excludes `linux_support.cpp` and `pathmatch.cpp` |
| `tier0/tier0.vcxproj` | `tier0.dll` | 19 | Tier0 source, ordinal export definition, assertion-dialog resource |
| `utils/vgui/vgui.vcxproj` | `vgui.dll` | 70 | VGUI1 source/headers, exports, generated compatibility import library; this is retained runtime code despite its `utils/` location |
| `vgui2/vgui2.vcxproj` | `vgui2.dll` | 31 | `vgui2/src/`, public interface code; selected key translation is `vgui2/src/vgui_surfacelib/vgui_key_translation.cpp` |
| `projects/vs2026/GameUI.vcxproj` | `valve/cl_dlls/GameUI.dll` | 128 | `GameUI/`, shared VGUI/engine code, CS bot/profile/UI support, DemoPlayer private common source; project reference to tier0; SDL/Steam |
| `projects/vs2026/particleman.vcxproj` | `valve/cl_dlls/particleman.dll` | 6 | Particle source, public interfaces, **forced Blue Shift headers**; preserve decorated exports |
| `HLTV/Core/Core.vcxproj` | `Core.dll` | 31 | Core's private public/engine/common/bzip2 source and shared containers; Winsock2 |
| `HLTV/DemoPlayer/demoplayer.vcxproj` | `DemoPlayer.dll` | 11 | DemoPlayer private support and shared containers; requests Core interfaces |

Preserve the established `valve/cl_dlls/` location of GameUI and particleman.
Counter-Strike also resolves these modules through the runtime filesystem/search
paths; a CS-only output directory is not an equivalent replacement.

## Optional: 3 projects, source retained

All three currently have **Debug and Release `Build.0` entries** in
`projects/vs2026/projects.sln`. They are presently built by that solution, not
already disabled upstream. The approved CMake runtime-focused defaults are
broadcasting **OFF** and A3D **OFF**; explicit feature options allow their builds.

| Project | Output / C/C++ entries | Dependencies and meaning of disabling |
| --- | --- | --- |
| `projects/vs2026/HLTV.vcxproj` | `hltv.exe` / 26 | Console, shared engine/VGUI controls, `HLTV/Console/hltv.rc`; loads filesystem/VGUI2 and `proxy001`. OFF omits the standalone broadcaster executable. |
| `projects/vs2026/Proxy.vcxproj` | `proxy.dll` / 33 | Proxy source, **Core's private bzip2/support source**, common containers, Winsock2 and Steam import library. OFF omits the broadcaster proxy; keep Core and DemoPlayer. |
| `projects/vs2026/a3dapi.vcxproj` | `a3dapi.dll` / 16 | `a3dapi/`, its `.def`/`.rc`, Windows COM/audio libraries; source loads legacy `A3D.DLL`. OFF omits this legacy adapter. No active A3D consumer was found in the selected engine source; end-to-end A3D support is unverified. |

Keeping optional source also protects `HLTV/Console/`, `HLTV/Proxy/`, their
private copied headers, and `a3dapi/`. Disabling broadcasting must not remove
engine demo playback or dedicated-server support.

## Proposed removal: 18 other-game projects

All project filenames in this table are under `projects/vs2026/`. Each row is
one project with disposition **remove**. **No retained or optional project was
found to consume any of these output DLLs or reference these projects.** Shared
source/header consumers are independent and are recorded in the final column.

| Project | Output | Exclusive implementation candidate | Shared dependencies / retained consumers |
| --- | --- | --- | --- |
| `opfor_cdll.vcxproj` | `gearbox/cl_dlls/client.dll` | `gearbox/` client source | Uses shared common/game code, SDL, and retained VGUI1; keep those dependencies |
| `opfordll.vcxproj` | `gearbox/dlls/opfor.dll` | `gearbox/` server/movement source | No retained source consumer found inside `gearbox/` |
| `bshift_cdll.vcxproj` | `bshift/cl_dlls/client.dll` | Blue Shift client/private UI implementation only | Uses shared VGUI/engine/public/common source and SDL; **particleman consumes Blue Shift headers** |
| `bshiftdll.vcxproj` | `bshift/dlls/hl.dll` | Blue Shift server implementation only | Preserve `bshift/` pending a file-level split because of particleman's forced/transitive includes |
| `tfc_cdll.vcxproj` | `tfc/cl_dlls/client.dll` | TFC client implementation only | Uses retained VGUI1, shared common/game code, SDL/Winsock2; HL viewport has a guarded TFC-header include |
| `tfcdll.vcxproj` | `tfc/dlls/tfc.dll` | TFC server implementation only | Preserve `tfc/` in the initial source pruning; guarded header closure unresolved at file level |
| `dod_cdll.vcxproj` | `dod/cl_dlls/client.dll` | DoD client/private UI implementation only | SDL/OpenGL/Winsock; **CS's private interface header unconditionally includes a DoD interface header** |
| `doddll.vcxproj` | `dod/dlls/dod.dll` | DoD server implementation only | Preserve `dod/` initially; also has conditional HL voice-header edges |
| `dmc_cdll.vcxproj` | `dmc/cl_dlls/client.dll` | DMC client implementation only | Uses retained shared game/common/public code, VGUI1 and SDL; conditional HL voice-header edge |
| `dmcdll.vcxproj` | `dmc/dlls/dmc.dll` | DMC server/movement implementation only | Uses shared `game_shared/voice_gamemgr.cpp`; preserve `dmc/` initially pending conditional/include audit |
| `ricochet_cdll.vcxproj` | `ricochet/cl_dlls/client.dll` | `ricochet/` client source | Uses shared game/common/public source, VGUI1 and SDL; keep those dependencies |
| `ricochetdll.vcxproj` | `ricochet/dlls/mp.dll` | `ricochet/` server/movement source | Uses shared voice source; no retained source consumer found inside `ricochet/` |
| `gunman_cdll.vcxproj` | `rewolf/cl_dlls/client.dll` | References `gunman/src/cl_dll/` | Referenced `cdll_int.cpp` is absent; no `gunman/` directory exists to prune |
| `gunmandll.vcxproj` | `rewolf/dlls/gunman.dll` | References `gunman/src/dlls/` | Referenced `h_export.cpp`, `cbase.cpp`, `game.cpp` are absent; keep export manifest until archival step |
| `czero_cdll.vcxproj` | `czero/cl_dlls/client.dll` | **None** | Imports the exact same 168-entry `cstrike-client.items` used by retained CS; SDL/Steam remain required |
| `czerodll.vcxproj` | `czero/dlls/mp.dll` | **None** | Imports the same 136-entry `cstrike-server.items`; all CS bot/hostage/gameplay source remains |
| `czeror_cdll.vcxproj` | `czeror/cl_dlls/client.dll` | `czeror/` client/gameplay source | Imports `czeror-client.items` but also compiles CS public/common/game/UI source; preserve `cstrike/` and shared VGUI/SDL dependencies |
| `czerordll.vcxproj` | `czeror/dlls/cz.dll` | `czeror/` server/movement source | Imports `czeror-server.items`; no retained source consumer found inside `czeror/` |

`engine/sys_sdlwind.cpp` still recognizes client path strings for other games.
Those compatibility branches are shared engine code, not output dependencies
that require building the removed games. Their removal is not proposed.
Likewise keep `engine/cz*` source, including compiled `czmodelcheck.cpp`.

## Proposed removal: software client renderer and editor

| Project | Output | Source pruning boundary / dependency result |
| --- | --- | --- |
| `projects/vs2026/sw_engine.vcxproj` | `sw.dll` | Remove the software **client** target only. Its 187 active C/C++ paths are also selected by retained `swds`; there is **no exclusive compile source to delete**. Keep engine software-renderer/media code, `engine/engine.rc`, VGUI1, codecs and Miles. |
| `worldcraft/worldcraft.vcxproj` | `utils/hammer.exe` | Hammer's `worldcraft/` implementation/resources are a source-pruning candidate. No retained/optional source, output-library, or generation dependency was found. Its MFC/OpenGL/GLU/WinMM/RPC requirements do not become runtime requirements. |

The launcher still exposes/selects `sw.dll` (`launcher/launcher.cpp`), and engine
video settings still write that name. Removing this target intentionally makes
software client mode unavailable. Any later presentation of that unavailable
mode must be handled explicitly; changing launcher behavior is not part of this
Phase 1 document. `swds.dll` is loaded by `dedicated/sys_ded.cpp` and remains.

## Proposed removal: 18 SDK utility projects

All project filenames are under `projects/vs2026/`. Each has disposition
**remove**. No retained/optional target consumes their output or invokes these
tools as a build-time generator. Shared support source and prebuilt libraries
are not automatically removable with the tool.

| Project | Output | Exclusive source candidate | Shared inputs / dependency notes |
| --- | --- | --- | --- |
| `bspinfo.vcxproj` | `utils/bspinfo.exe` | `utils/bspinfo/` | Compiles `utils/common/` BSP/cmd/script source; preserve common tree |
| `light.vcxproj` | `utils/light.exe` | `utils/light/` | Common BSP/math/script/thread source |
| `makefont.vcxproj` | `utils/makefont.exe` | `utils/makefont/` | Common cmd/WAD source; WinMM |
| `makels.vcxproj` | `utils/makels.exe` | `utils/makels/` | No retained source/output consumer found |
| `mdldec.vcxproj` | `utils/mdldec.exe` | `utils/mdldec/` | Tool-private math/export source; CRT `legacy_stdio_float_rounding.obj` |
| `mdlviewer.vcxproj` | `utils/mdlviewer.exe` | `utils/mdlviewer/` | Common math source; GLUT import library/DLL, OpenGL/GLU |
| `mkmovie.vcxproj` | `utils/mkmovie.exe` | `utils/mkmovie/` | No retained source/output consumer found |
| `procinfo.vcxproj` | `bin/obj/procinfo/Win32/<Configuration>/procinfo.lib` (SDK-relative) | `utils/procinfo/` | **Static library**, not an executable; includes managed framework reference entries; no retained library consumer found |
| `qbsp2.vcxproj` | `utils/qbsp2.exe` | `utils/qbsp2/` | Common support; glaux/OpenGL/GLU and `legacy_stdio_definitions.lib` |
| `qcsg.vcxproj` | `utils/qcsg.exe` | `utils/qcsg/` | Common support; glaux/OpenGL/GLU and `legacy_stdio_definitions.lib` |
| `qlumpy.vcxproj` | `utils/qlumpy.exe` | `utils/qlumpy/` | Common cmd/LBM/script/WAD source |
| `qrad.vcxproj` | `utils/qrad.exe` | `utils/qrad/` | Common BSP/math/poly/script/thread source |
| `serverctrl.vcxproj` | `utils/serverctrl.exe` | `utils/serverctrl/` | Tool-private MFC source, `ServerCtrl.rc`, WinMM; separate from retained dedicated admin UI |
| `smdlexp.vcxproj` | `utils/smdlexp.dle` | `utils/smdlexp/` | 3ds Max plugin, `.def`/`.rc`, Max SDK `maxutil/geom/mesh/core` libraries and COMCTL32; not an HLTV Core consumer |
| `sprgen.vcxproj` | `utils/sprgen.exe` | Tool implementation only; **preserve directory** | Common cmd/LBM/script source; retained engine directly includes `utils/sprgen/spritegn.h` |
| `studiomdl.vcxproj` | `utils/studiomdl.exe` | `utils/studiomdl/` | Common cmd/LBM/math/script/trilib source; not an automatic runtime build step |
| `vis.vcxproj` | `utils/vis.exe` | **`utils/visx2/`** | Actual selected implementation is `visx2`, not `utils/vis/`; common support remains |
| `xwad.vcxproj` | `utils/xwad.exe` | `utils/xwad/` | Common cmd/LBM/WAD source |

Authoring assets externally may still require these tools. Their exclusion is a
repository-scope decision, not a claim they are unnecessary to the ecosystem.

## Source deletion proposal and protected paths

Perform target removal first. After the retained manifest/CMake comparison,
the following **21 roots** are candidates for deleting exclusive implementation
and tool/game-private assets, preserving all license/provenance files and
original manifests until their audit is complete:

```text
gearbox/                    ricochet/                  czeror/
worldcraft/
utils/bspinfo/              utils/light/               utils/makefont/
utils/makels/               utils/mdldec/              utils/mdlviewer/
utils/mkmovie/              utils/procinfo/            utils/qbsp2/
utils/qcsg/                 utils/qlumpy/              utils/qrad/
utils/serverctrl/           utils/smdlexp/             utils/studiomdl/
utils/visx2/                utils/xwad/
```

No retained/optional manifest-selected source or conservative include-closure
consumer was found inside these roots. This is a bounded finding at the recorded
HEAD, not permission for a blind recursive deletion: check notices, nested
resources and original manifests when preparing the actual deletion diff. No
other source-directory deletion is proposed. In particular:

| Preserve initially | Concrete reason |
| --- | --- |
| `bshift/` | `particleman.vcxproj` compiles `public/interface.cpp` with `BASEMONSTER_H`, Blue Shift include precedence, and forced `bshift/dlls/extdll.h`, `util.h`, `cbase.h`; their transitive header closure is retained |
| `dod/` | `cstrike/public/game_controls/iclientvgui.h` unconditionally forwards to `dod/public/game_controls/iclientvgui.h`; conditional voice includes are an additional conservative edge |
| `tfc/`, `dmc/` | `cl_dll/vgui_TeamFortressViewport.h` includes TFC definitions under `_TFC`; `cl_dll/voice_status.cpp` has DMC/DoD branches. These are conditional source edges, **not active HL dependencies proved by preprocessing**. Defer file-level cleanup rather than deleting their trees now. |
| `utils/common/` | `engine/bspfile.h` forwards to its `bspfile.h`; both HL and CS `dlls/animation.cpp` include its `mathlib.h` |
| `utils/sprgen/` | `engine/spritegn.h` forwards to `spritegn.h`; tool-target removal does not permit deleting this format header |
| `cstrike/`, `cl_dll/`, `dlls/`, `game_shared/`, `pm_shared/`, `common/`, `public/` | Retained client/server/UI code and shared interfaces; CZ project removal yields no authority to delete shared CS code |
| `engine/`, `swds/`, `dedicated/` | Both engine variants and dedicated launcher remain; software client source is shared with `swds` |
| `utils/vgui/`, all retained `vgui2/` source | Runtime libraries/controls, generated VGUI import library, and selected VGUI2 key translation; not removable SDK utilities |
| `HLTV/common/`, Core/DemoPlayer private copies, Console/Proxy source | Engine demo playback, GameUI compilation, and preserved optional broadcaster |
| `external/`, `lib/`, `deploy/`, `tracker/`, `network/`, `linux/`, `devtools/` | No blanket cleanup proposed; retain dependencies, runtime resources, compatibility headers, alternative manifests and provenance until their own audit |

The conditional TFC/DMC edges do not require retaining their game **targets**.
They explain why this proposal is more conservative about deleting source than
about excluding targets.

Keep `ricochet/cl_dll/readme.txt` as source-context documentation when preparing
the Ricochet deletion diff. Its contents describe inherited client interfaces;
they do not supply a new license grant. Filename searches for notices cannot
replace inspection of embedded notices in the files actually being removed.

## Retained link, generation and runtime dependencies

### Imports and build events

`CounterStrike.props` supplies output layout, include ordering, configuration
CRT/optimization rules, client SDL linkage, and CS-client Steam linkage. Both
CS projects remove inherited `NDEBUG` and enable `UTL_ENGINE_ASSERTS`; their
`_ITERATOR_DEBUG_LEVEL=0` must survive. `VisualStudio2026.props` selects the
toolset/SDK for its importers and adds utility warning compatibility definitions.
The four imported source lists are `cstrike-client.items`,
`cstrike-server.items`, `czeror-client.items`, and `czeror-server.items`.

The only nonempty build-event command found is VGUI1's **retained** post-build
`lib /nologo /machine:x86 /def:...vgui_imports.def /out:...vgui.lib` compatibility
import-library generation. No project has a `CustomBuild` item or an invocation
of a proposed-removed utility as a generator. Empty post-build placeholders do
not establish an asset-generation dependency. This does not assert the original
MSBuild toolchain produces no ordinary generated intermediates/resources.

Direct retained project references are `hw_engine`, `swds`, and `hl_cdll` to
VGUI1, plus GameUI to tier0. These references alone are not the runtime graph.
The HL client also contains an explicit **`Library` item** for
`lib/public/game_controls.lib`, outside `AdditionalDependencies`.

### Libraries and providers to preserve

| Family | Retained consumers and selection |
| --- | --- |
| Miles | `hw_engine` and `swds` link `external/miles/mss32.lib`; keep `engine/cd.cpp`, `thread.cpp`, queue/worker/command paths and Miles terms. COFF import members identify x86 `mss32.dll`. |
| SDL2 / Steam | `lib/public/SDL2.lib` and `steam_api.lib` are x86 import libraries identifying `SDL2.dll` and `steam_api.dll`. Used by engines, CS client, GameUI, HL client (SDL), and optional Proxy (Steam), as selected in their manifests. |
| Codec/compression/font libraries | Engines use `external/installed/x86-windows-static-md/lib` in Release and `debug/lib` in Debug: brotli decoder/common, Vorbis/Vorbisfile/Ogg, WebM/VPX, plus `bz2`/`bz2d`, `freetype`/`freetyped`, `libexpatMD`/`libexpatdMD`, `libpng16`/`libpng16d`, `zs`/`zsd`. Preserve architecture, variants and transitive dependencies. |
| Winsock | Engines' `net_ws.cpp` is selected by hardware and `swds`, both linked to **`wsock32.lib`**. HL client also lists WSOCK32. Core, launcher, hlds and optional Proxy use **`ws2_32.lib`**. Keep the C2 backend/constant distinction. |
| Windows graphics/audio | Hardware lists OpenGL32, WinMM, DXGUID, DirectSound. `swds` lists DDraw, WinMM, DXGUID, DirectSound. Preserve target-specific Windows imports; their presence does not authorize running graphics/audio. |
| Bundled UI libraries | `game_controls.lib` and `vgui_controls.lib` contain ordinary x86 COFF object members rather than short import members. HL client explicitly uses the former; keep both pending the detailed link audit. The VGUI1 generated import library is a separate artifact. |
| Private support source | Core compiles private bzip2 and copied engine/public code; DemoPlayer and GameUI use private copied support. Optional Proxy also uses Core's private source. Keep original ownership and include precedence. |

The three identified runtime import DLLs are not bundled in this checkout;
outside installed package tooling, the only `.dll` found is the utility GLUT
DLL. Required compatible provider availability, redistribution permissions and
Debug/Release CRT compatibility remain to be established. The presence of
`external/installed/x64-windows` package tools is not an x64 runtime dependency
solution. No third-party directory/library deletion is included in this request.

### Runtime edges not represented by project references

| Source evidence | Runtime dependency / consequence |
| --- | --- |
| `launcher/launcher.cpp` | Loads `filesystem_stdio.dll` and the selected `hw.dll`/`sw.dll`; software mode becomes unavailable if its target is excluded |
| `dedicated/sys_ded.cpp` | Loads **`swds.dll`**, filesystem and VGUI2; console/GUI behavior and `deploy/platform/Admin/` resources remain |
| `engine/DemoPlayerWrapper.cpp` | Requests `demoplayer001` from `demoplayer`; keep DemoPlayer even with broadcasting disabled |
| `HLTV/DemoPlayer/DemoPlayer.cpp` | Requests `server001` and `world001` from `core`; keep Core independently of Proxy |
| `engine/vgui2/BaseUI_Interface.cpp` | Loads VGUI2, `cl_dlls/GameUI.dll`, and `chromehtml.dll`; Chrome provider has no project here and remains an unresolved runtime/package dependency |
| `cstrike/cl_dll/cdll_int.cpp`, `cl_dll/cdll_int.cpp`, `public/particleman.h` | Load particleman; CS additionally loads filesystem/GameUI and optionally `Bin/TrackerUI.dll` |
| `engine/cdll_int.cpp`, `engine/sys_dll.cpp` | Load game client/server modules through filesystem/game configuration; retain both CS and HL outputs and their proper relative paths |
| `engine/voice.cpp` | Loads codec-name DLLs dynamically; codec-provider supply is not established by the engine's static codec libraries |
| `HLTV/Console/System.cpp`, `HLTV/Proxy/Proxy.cpp` | Optional broadcaster requests `proxy001`; Proxy requests Core interfaces. Turning it off does not remove Core/DemoPlayer. |

Module names in strings are not proof every branch executes. Legacy/security,
Steam, diagnostic and optional hooks also exist (for example ModuleC,
TrackerUI, and guarded `ghostinj.dll` paths); this inventory does not mark them
as mandatory, remove them, or treat their availability as verified. No new
provider substitution, shutdown behavior, or stub is proposed.

## Export, resource and configuration constraints for the next phase

Preserve all selected inputs, including:

- `.def`: `launcher/hl.def`, `swds/swds.def`, `tier0/tier0.def`,
  `utils/vgui/vgui.def`, `utils/vgui/vgui_imports.def`, `dlls/hl.def`, and the
  CS/GameUI/particleman definitions under `projects/vs2026/`. Optional A3D uses
  `a3dapi/a3dapi.def`. Export attributes/linker options also matter where no
  `.def` is selected: hardware must not acquire an unrelated export manifest.
- `.rc`: `launcher/hl.rc`, `engine/engine.rc` (both engines),
  `dedicated/hlds.rc`, `tier0/assert_dialog.rc`; optional HLTV and A3D have their
  own resources. Keep nested resource inputs and embedded manifests.
- VGUI1's ordinal/data alias at `@395`, Core's `CreateInterface @1`,
  particleman's decorated C++ export, and other export names/ordinals. Do not
  replace these with indiscriminate symbol auto-export.
- Hardware uses precise FP, strict FP for `engine/common.cpp`, async EH for
  `sys_dll2.cpp`, SSE2 and disabled Release COMDAT folding. `swds` also has the
  strict/async overrides and disabled folding. Do not copy a global ICF/fast-FP
  policy into them.
- CS's DLL CRT selection differs from Half-Life's static CRT selection; VGUI1
  and A3D also use static CRT variants. Tier0 uses async EH. Assertions remain
  enabled in several Release projects, including hardware, CS, GameUI, tier0
  and VGUI2. CMake Release defaults must not silently add `NDEBUG` everywhere.
- Preserve per-file forced includes and include order (especially particleman),
  object-name collision overrides, `hlds`'s C-as-C++ setting for `engine/mem.c`,
  exclusions for engine OpenAL source and GameUI `TestScriptController.cpp`,
  calling convention/packing/subsystem defaults and any evaluated SDK settings.

A dedicated-build issue needs explicit follow-up: the current `swds.vcxproj`
defines `SW_RENDERER`, but does **not** explicitly define `SWDS_DLL`, while
`engine/sys_dll.cpp` and other shared source use `SWDS_DLL` conditionals. Its
export file also says `LIBRARY sw` while the target is `swds.dll` (the project
suppresses linker warning 4070). These are recorded discrepancies, not changes
authorized by a filename or proof the dedicated runtime works. Resolve the
evaluated flags/export behavior during the CMake audit without weakening gates
or silently altering dedicated semantics.

The retained/optional resource scripts were also checked for file-backed icon
inputs. These five files exist and must remain: `launcher/hl.ico`,
`engine/res/engine.ico`, `engine/res/engine_112.ico`, `dedicated/hlds.ico`, and
`HLTV/Console/hltv.ico`. The engine resource additionally references the
launcher's icon through `../launcher/hl.ico`. None is under a proposed source
deletion root. A3D's resource is version metadata without a separate icon input.
This checks source-file availability, not resource compilation or rendering.

## Approval boundary and remaining work

Phase 1 initially changed only this document. After owner approval on 2026-11-06,
the 38 project targets were removed, their original manifests archived, CMake
translation and C1–C4 implemented, and 1,330 exclusive files pruned within the
approved roots. See `source-pruning.md`, `building.md`, `source-corrections.md`,
`licensing.md` and `validation.md` for the actual implementation and limits.
Changes are committed locally; no pushes or releases were made.

The approved execution sequence was:

1. Exclude the approved 38 targets from the active build while preserving the
   original manifests as translation evidence.
2. Audit/evaluate retained Debug/Release source, flags, libraries, resources and
   exports; translate the 16 default targets and preserve options for the 3
   optional projects. Keep dedicated support in the complete-runtime preset.
3. Prepare source deletions only within the approved exclusive-source boundaries,
   with a reviewable file diff and preserved notices. Leave protected/shared
   directories intact; further pruning there needs separate evidence/scope.
4. Perform the licensing/origin work, hardware-only CMake entry point, C1–C4 and
   architecture-portability inventory from the handoff. Their implementation and verification limits are
   reported separately; this inventory alone is not proof of completion.

Full MSBuild evaluation, compilation, PE/import/export validation, provider
packaging and runtime behavior remain unverified. Tests and runtime execution
remain prohibited by the handoff. The handoff explicitly requires owner
approval of this pruning list before deletion or implementation; that approval
was supplied on 2026-11-06.
