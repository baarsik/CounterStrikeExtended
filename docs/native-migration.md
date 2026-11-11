# Native x64 and ARM64 migration

The migration preserves Counter-Strike 1.6, Half-Life, their UI/media, dedicated
servers and HLTV. Win32 remains the maintained reference and default. **No native
target has completed acceptance.** See [build commands](building.md),
[validation evidence](validation.md) and [component tests](../tests/native/README.md).

## Targets

| Target | Commitment | Current state |
| --- | --- | --- |
| Windows 11 x64 | Required; first complete native target | All 19 targets build; selected CS/HL runtime checks pass on VM1 under x64 emulation |
| macOS 14+ Apple Silicon | Required | All 19 runtime roles build in a frozen Release/Debug cohort; fresh CS lifecycle and HL save-marker roundtrip pass on Apple Silicon. Current-source combined acceptance and production Metal remain |
| Windows ARM64 | Conditional | Native components pass selected checks; full providers/gameplay remain unqualified |
| Linux x64 / ARM64 | Opportunistic | Reuse shared POSIX/SDL implementations only; defer substantial platform/provider work |

Windows ARM64 must work on available hardware or a qualified Parallels graphics
path. A necessary x64 provider helper may use a versioned, pointer-free protocol;
it must retain full provider behavior and clean failure/shutdown. No additional
renderer project or hardware purchase is part of conditional-target work.

## Compatibility requirements

- Native clients must play on unchanged Win32 ReHLDS servers running Metamod and
  AMX Mod X. Preserve protocol 48, delta/user-command/message encoding,
  authentication, consistency, downloads, menus, HUD, sounds and voice. Integrity
  policies must not be bypassed.
- Rebuild owned native modules together. Original x86 DLLs and executable blobs
  cannot load in a native process. Compatibility does not require loading their
  executable code or round-tripping files with untouched Valve binaries.
- Saves, demos and navigation must round-trip between native targets and the
  updated project Win32 build. Keep legacy envelopes, symbolic field/callback
  names, entity indices, fixed path lengths and serialized widths.
- Keep gameplay algorithms and per-file floating-point, exception and assertion
  policies stable. Native storage size must not change wire or disk encodings.
- Preserve VGUI widgets/resources/layouts, startup video, browser MOTD, music,
  effects, spatial/geometry behavior and microphone/voice features. SDL3 provides
  platform I/O; it does not replace these algorithms or providers by itself.
- Map legacy CD track commands to digital music; physical optical-drive support
  is excluded. Developer builds and a local Mac app bundle are in scope;
  installers, publication and notarization are deferred.

## Implemented boundaries

| Area | Implementation and evidence | Remaining work |
| --- | --- | --- |
| Build/dependencies | One generated inventory from retained manifests; explicit OS/CPU/compiler policies; source-built SDL3 3.4.20, mpg123 1.33.7 and pinned media/font/compression libraries; source-built CS controls | Fresh complete builds after later source edits; conditional-target qualification |
| Module ABI | `CseGetModuleAbi` checks architecture, pointer/compiler profile, role and consumed table signatures for selected game/client/filesystem exchanges; runtime ABI 10 | Other C++ boundaries and rejection before all static callbacks |
| Data | Native handles/string storage, typed callback registration, explicit Win32 demo/navigation records, symbolic C4 phase, HPAK records and bounded custom-decal parsing | Full entity/transition/C4/nav writing, VFS/spray integration and Mac game round trips |
| Platform/numerics | Owned module loading, cooperative workers, UDP components, CPU/timer discovery, scalar packed lighting, legacy conversion helpers and native client video/bootstrap compilation | Current-cohort dedicated/admin runtime, complete client restart/unload and platform acceptance |
| Audio | mpg123 music, SDL DMA/capture components, retained mixing/DSP rules and native voice-mixer component | Native A3D software provider and public device enumeration are implemented; selected Speakers and DMA/music coexistence pass R/D. Default RODE HAL startup, acoustic delivery/capture/voice, complete spatial/OEM parity and measured cross-CPU conversion differences remain |
| Steam/CEF | Owned Steam service and sandboxed CEF controller/host; real provider fixtures on Windows x64 and Mac ARM64 | Full game/browser/voice integration and provider failure/lifetime acceptance |
| UI/text/input | Native Mac App/System, SDL VGUI1 Surface, selected CoreText/FreeType provider, CP1252 clipboard bridge and UTF-16 VGUI2 reads; actual-library drawing/clipboard/key translation checks | Embedded VGUI1 root admission, default font selection/layout parity, physical input, standalone-window event routing and quiescence |
| Renderer | Private command/state/texture layer; three selected D3D12 raster profiles and one Metal profile match frozen GL RGB bytes; offline DXBC/metallib tooling | Production wiring, unsupported legacy branches, full world/model/UI/media presentation and parity |

The ABI descriptor does not make every module safe to exchange native C++ objects.
The AppCold loader, experimental clipboard owners and alternate TextEntry lifetime
components remain unselected until their real host ownership and shutdown prerequisites
hold. The selected client App/System use synchronous SDL clipboard copies; the
native OS publication is text, without Windows allocation padding or embedded-NUL tails.
Font lookup or raster availability alone does not qualify layout or readable UI.

## Remaining work and gates

1. **Close interfaces and serialization.** Finish native C++ module boundaries,
   reject mismatches before callback exchange, and run high-address fixtures.
   Separate native entity offsets from fixed-width network/save identifiers.
   Verify every save callback, transition and C4 phase, including legacy imports
   reconstructed from attenuation. Complete bidirectional demo/save/nav tests.
2. **Complete native platform owners.** Finish dedicated/admin and A3D dependencies
   and qualify the compiled client owners. Preserve VFS search order,
   game/fallback paths, case behavior, settings, directories, clipboard and shell
   functions. Bind real font, input and clipboard services with explicit UTF-16
   boundaries and owner lifetimes. Join workers before releasing their code.
3. **Qualify providers in the game.** Preserve Steam sessions/authentication,
   callbacks, server functions and encoded voice. CEF must retain off-screen
   pixels, input, cookies/cache and MOTD. Keep writable data in user locations.
   Missing voice, authentication stubs or text-only MOTD do not satisfy this gate.
4. **Finish the shared renderer.** Retain Windows OpenGL for comparison; implement
   the shared command/state contract through SDL_GPU D3D12/Metal, with offline
   shaders. Cover fixed-function state/topology, world/models/sprites, lightmaps,
   fog/transparency, decals/particles/triangle APIs, VGUI/browser/video textures,
   screenshots, MSAA and display controls. Select SDL_GPU only after parity.
5. **Accept mandatory targets.** Run complete Windows x64 first, then Mac ARM64,
   including local app-bundle helpers/library paths and writable-data separation.
   Require Release and Debug evidence below before promotion.
6. **Evaluate conditional targets.** Check actual ARM64 images/native execution and
   complete provider/gameplay behavior. Linux may add small build/naming/package
   changes using POSIX, SDL and existing providers; defer either architecture if
   a new backend/provider or substantial platform work is necessary.

All 19 runtime responsibilities remain in the acceptance inventory. Keep libraries,
PDBs, import archives, tests and intermediates outside runtime outputs; Release
symbols and distribution materials remain opt-in. After mandatory acceptance,
make Windows x64 the default while preserving the root runtime-output contract
and isolating explicit Win32 reference outputs.

### macOS runtime checkpoint, 2026-11-10

The user accepted smooth fast turns and scrollwheel bhops without a visible
pointer, retained fullscreen after changing 1080p to 1440p in Counter-Strike,
and the corrected timer colon with normal FPS/textures. Normal Counter-Strike
sound and Half-Life music before/after app switching were also confirmed.
These observations belong to their pinned test cohorts; they do not qualify
later combined images automatically. The production client still uses OpenGL.
A separate combined-image test selected Apple's software renderer and regressed
FPS/surfaces; restoring the earlier engine tuple removed both symptoms. The
selection cause remains open, and new hardware tests must verify the actual GPU.

The reviewed Half-Life multisource compatibility repair restored the Unforeseen
Consequences scene and movement, as confirmed by the user. Actual owning source
and fixtures passed native Release/Debug/sanitizer checks and Windows Win32/x64
Release/Debug checks. A later supervised Mac run loaded the autosave on the
M5 Max, accepted movement, returned through the real Leave game menu, and exited0
after a normal application-quit request at eight minutes. This bounded success
does not close the older Leave game ED_Alloc report, all save/decal cases, or
prior unexplained exits. Earlier direct launchers did not retain child status;
future runtime phases use detached persistent supervisors.

The Control-Space provider and gameplay/UI ownership integration are reviewed.
Native Half-Life and Counter-Strike runs preserve the layout during gameplay,
permit switching in observed chat/console/menu contexts and record simultaneous
duck+jump commands. Half-Life also survives two same-PID fullscreen restart
commands; both supervised tests quit normally with exit0 and restore settings.
Windows qualification covers 60 actual owning compiles and 20 fixture/factory
cases. These are native UI-driven functional checks, with no new human physical
mouse/performance acceptance; see the [keyboard design](superpowers/specs/2026-11-10-macos-gameplay-shortcuts-design.md).
The screenshot interaction-owner query detects the real native selected-portion
overlay with the game-style window still focused and returns ownership after
cancellation/Capture. The engine capture coordinator is reviewed and integrated. Its native checks
show capture released during the actual OS overlay and reacquired after Capture
or Cancel on the M5 Max; Windows qualification passes40owning compiles and24tests.
A valid selected-portion clipboard image was inspected. Physical Cmd-Shift-4
region dragging remains open because the UI automation cannot address the
WindowServer selection surface. Native UI checks performed while the user is
away are not human mouse/performance acceptance.

The current Counter-Strike keyboard test renders on the M5 Max at 1440p fullscreen
and displays MOTD HTML, but revealed garbage after developer notify text.
Actual-source sanitizers reproduced a notify-buffer overread at 1440p. The
reviewed bound/NUL repair passes native90cases, coherent Release/Debug engine
links, Windows8owning compiles/80cases and live CS1440p long/short/cleared
notification checks. Those bounded checks do not qualify all HUD rendering. Local listen gameplay does not qualify
external Steam authentication; the console reports validation stalled. The
current app layout also fails strict embedded-main resource verification while
the exact standalone image bytes verify, so distribution packaging remains open.
A retained CS110 CEF helper report records a launch-time SIGTRAP; its precise
cause and relationship to the existing browser code-identity warning remain
unresolved. Packaging also needs to account for relative `hl.conf` and
`steam_appid.txt` writes as well as the VFS configuration/save/download paths.
Successful MOTD rendering does not qualify every browser helper or final signing.

A compiler-idle Release CS listen run records uneven frame delivery after a
90-second warm-up:11,121focused gameplay frames over142seconds have median
10.924ms/p99 24.841ms. Removing the broad CUA input interval leaves p99 24.877ms.
Simulation/audio measurements are small; graphics and unmeasured outer-loop/OS
scheduling require further attribution. Ordinary background apps/VMs/capture
services stayed running. This is a measured lead, not proof of a renderer cause
or a multiplayer/performance acceptance.

The native Find Servers module/UI and checked host retirement are implemented,
reviewed and integrated as source. Native Release/Debug owning links and real
VGUI/module/component checks pass, including large-list close/reopen, region/tab
identity, independent status messages and selection during rapid tab changes.
The reviewed 54e360fd candidate is staged in both owned game apps. An actual
Counter-Strike menu test opens Servers and populates Internet rows, but exposes
unreadable controls under the game's HD parent and a native App-menu shutdown
crash. The shutdown fault follows a detached embedded root into the VGUI input
fallback. The independently reviewed native64 input guard is now integrated and
staged as VGUI2 with the existing 54e GameUI/engine/main. Both games open Servers
and complete native application-menu Quit with supervised OS exit 0; Half-Life
also visibly populates Internet rows. These checks deliberately exclude audio,
physical input and performance. The reviewed e7eb139 layout repair is now
integrated and staged too. Actual Counter-Strike and Half-Life checks show
readable HD controls, separated tabs and headers, live Internet rows and keyboard
access to lower controls; both quit normally. Physical close/reopen, full live
connection/authentication and Windows owning/reference qualification remain open. The final
real Steam model probe discovers rows for app10/app70, fully drains storage with
CloseStep(128), pumps callbacks after release and shuts down normally. Read-only
favorite/history snapshots remain equal before/after both phases. These checks
do not qualify live UI connect/authentication or accepted-history/password behavior
in a complete game. Standalone server lifetime,
A3D/spatial behavior, full renderer presentation, cross-target data round trips,
voice/authentication and current-cohort packaging/provider acceptance remain
part of the migration gates above. The microphone issue below is an explicit
user-directed deferral, not a completed fix.

The native A3D software provider, buffers, workers, resource/event ownership and
public copied-GUID device enumeration are now implemented and reviewed. The
frozen `6f27bef` cohort builds all 19 Mac runtime roles in both configurations;
`9498efa` adds the reviewed enumeration/selection harness cohort. Actual Release
and Debug probes select Mac Studio Speakers while the system default remains the
RODECaster, then separately prove DMA/music coexistence under a temporary
Speakers default. All four bounded cases restore or preserve the original route,
input, system output, volume and mute. Successful PCM submission is not acoustic
audibility, hardware removal/Restore, game-consumer routing or OEM driver parity.
The unchanged RODE HAL startup stall remains open. The stock software A2D
pitch/direct-channel reference and its omitted EQ/reflection records remain
bounded evidence, not an OEM acceptance claim.

Fresh `6f27bef` Counter-Strike completes map/reload/quit normally on the M5 Max
OpenGL renderer. Half-Life completes a same-build canonical 260-byte archive
roundtrip: actual saved player armor is 0, then 15 after a controlled grant,
0 after map reset, and 15 after load; the weapon mask is restored too. This does
not establish complete entity/callback/campaign or cross-platform save parity.
The CS wrapper's stale CF receipt was rejected before restoration writes and was
recovered separately; that failed wrapper remains recorded. The corrected HL
wrapper restores all three settings domains and the original output normally.
Both cases preserve the original assets and app baseline, SDK history records
match before/after, and old encrypted browser-cache files remain unchanged.
Browser HTML, physical audibility and performance are separate outstanding gates.

### Deferred: microphone consent blocks initial loading

**Status: deferred at the user's request, 2026-11-10; no implementation change.**
On first launch of the macOS Counter-Strike test client, the microphone consent
dialog appears while the game window is black and prevents initial loading from
finishing until the user responds. This was reported with the
`CounterStrikeExtendedMouseCS110` test bundle. Check Half-Life and the stable
application identity when implementing the fix; their exact first-launch behavior
has not been established by this report.

Expected behavior: load the menu without requiring microphone authorization, then
let the user explicitly enable microphone/voice access afterward. Pending or
denied permission must not make ordinary startup, gameplay or audio output depend
on granting microphone access. Preserve the user's OS permission choice.

The current SDL capture initialization opens and resumes a recording stream in
`GoldSrc-SDK/engine/voice_record_sdl.cpp` before `RecordStart()`. Investigate
decoupling capture-device/permission acquisition from startup. The exact blocking
call has not been profiled; this source path is a follow-up lead, not a completed
root-cause diagnosis. Later acceptance should cover first launch, pending/denied
permission, enabling voice from the loaded menu, later authorization and restart.
Track this as an explicit deferred exception in migration reports; granting
consent during a test does not resolve the startup issue.

## Promotion evidence

For each target, keep compilation, static inspection, module loading, gameplay and
external-server evidence separate. Require Release and Debug checks for:

- Actual binary architecture, exports/imports/resources, module mismatch rejection
  and strings/handles/allocations above 4 GiB.
- Win32 ↔ Windows x64 ↔ Mac demos, saves and navigation, including entity handles,
  symbolic callbacks, transitions and every C4 phase.
- Seeded movement, prediction, collision, RNG, weapons, bots, bomb/defuse and HL
  entities. Compare exact encoded bytes and gameplay decisions; no blanket float
  tolerance may conceal differences.
- Renderer scenes, mixer PCM, capture/microphone/voice, Unicode/IME, cursor/focus,
  DPI/controllers, browser behavior and repeated startup/shutdown.
- Listen/dedicated servers and HLTV; reconnect/changelevel, downloads, consistency,
  AMXX menus/messages/cvar queries and voice against the unchanged Win32 fixture.
  Add actual user server configurations when supplied.

Use isolated, authorized runtime/provider/network fixtures. A buildable target,
a runnable target and a qualified target are distinct states.
