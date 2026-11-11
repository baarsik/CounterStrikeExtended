# Validation status

No native target is fully qualified. The table records selected frozen cohorts;
later source edits require fresh build/runtime evidence. Windows results use
VM1, Windows 11 ARM64 running x86/x64 code under emulation. Mac fixtures run on
Apple Silicon with a macOS 14 deployment target; this is not a macOS 14 hardware
runtime qualification. [Remaining gates](native-migration.md#remaining-work-and-gates)
and [reproduction tools](../tools/qualification/README.md) are separate from these results.

## Native migration cohorts

| Evidence | Result | Limits |
| --- | --- | --- |
| V18 Win32 / Windows x64 Release and Debug | All 19 targets build; 21 Win32 and 30 x64 PE images per configuration; output/import/export/resource checks pass | Native Debug was interrupted at the disk reserve and resumed with identical inputs. Later HL length changes have owning-file/component checks, not a new complete cohort |
| V17 CS/HL x64 Release and Debug | Four selected gameplay/recording/full-replay cases; 651 frames and 15 pinned loaded images per case | Selected flows only; presentation, lifecycle, voice and numerical acceptance remain incomplete |
| V17 external Win32 server, x64 Release and Debug | Steam authorization, AMXX menus/HUD/messages/cvar replies, downloads, consistency and alive movement; 124 nonempty frames and 14 pinned loaded images per case | Unchanged ReHLDS 3.15.0.896 / Metamod-R 1.3.0.149 / AMXX 1.9.0.5303. No voice/audibility or live VAC-secure-bit claim |
| V11 data and server lifecycle | Eight selected Win32↔x64 demo replays, four HL save legs and selected reconnect/changelevel cases | Not complete entity, transition, C4 or navigation acceptance; not Mac game round trips |
| V18 production CS camera, x64 Release and Debug | Each has 1,041 alive frames, 199 movement-command frames and 410 exact rendered angle samples. Debug adds 18 read-only state samples. Separate multiplayer case retains third-person rejection | Threaded single-player camera only; no internal query/warp-order, atomic snapshot, other-mode or Mac camera claim |
| SDL_GPU renderer components | Three selected Windows D3D12 profiles and first native Metal profile match 12,288 GL RGB bytes each; R/D and Mac instrumented CPU variants checked | Only readback row order normalized. Production backend is unselected; other branches, precision formats and scene coverage remain open |
| Mac ARM64 client, 2026-11-09 Release/Debug | Fresh `cse_client` builds link all 14 client responsibilities and CEF helpers; 13 actual dylibs load in separate processes, launcher architecture checked | Standalone dedicated/A3D owners remain unported. No complete game startup, embedded VGUI1 admission, scene rendering or gameplay acceptance |
| Mac actual-library UI, Release/Debug | Clock/arguments/date/disk, VGUI1 CP1252 and VGUI2 UTF16 clipboard reads, key translation, native font raster, inset borders, shared popup bitmaps and SDL video lease teardown pass | SDL dummy video/software renderer; physical input, General pasteboard publication and default font/layout parity are unqualified |
| Mac ARM64 user gameplay, 2026-11-10 Release | Half-Life launches into a rendered map and the user reports flawless mouse behavior. Counter-Strike runs a local match; the user confirms smooth fast turns, wheel bhops and menu transitions with no pointer, plus fullscreen retention from 1080p to 1440p | Selected hardware-renderer flows and normal user input settings only. Other raw/grab, software-renderer and focus/resize cases remain unqualified. The short recorded trace validates the recorder and typed engine channels, not physical mouse performance |
| Windows regression for Mac client changes | Ten current owning C++ files compile on VM1 in Win32/x64 Release/Debug; COFF checks retain the C++ vector-origin owners | Owning-file checks reuse the V18 compiler/dependency policies; not a fresh complete Windows link/runtime cohort |
| Build-tool/component checks | 106 build-tool tests and 19-project translator check pass; selected component matrices include R/D, high addresses and sanitizers | Individual fixtures define their covered inputs; they do not establish complete game or provider acceptance |

The frozen producing-source/package manifests, imported/loaded-image hashes,
failed attempts and reports are retained outside the repository under:

```text
~/.codex/artifacts/cse-native64-20261108/
```

Useful entries: `runtime-platform-v18-*`, `native-camera-production-v18-final/`,
`hl-ui-lengths-v1/`, `render-metal-v1/`, `xcode-metal-component-v1/`, and
`macos-runtime-build-native-audit-v19-*`. Their reports and SHA-256 indexes bind
claims to exact inputs. A private machine path is not a substitute for rerunning
the checked-in [fixtures](../tests/native/README.md).

The 2026-11-10 Mac gameplay records are in
`build/macos-runtime-bringup-20261109/`: `mouse-timing109-smoke4.json`,
`gameplay-cs110-baseline.json` and their completion receipts. The candidate uses
the reviewed mouse changes and genuine-main executable identity fixes; its
coherent engine/launcher overrides and retained image hashes are recorded in
`mouse-timing109-bootstrap-r1-stage.json`. This does not promote all 19 runtime
responsibilities or qualify a distribution package.

Open runtime findings include fullscreen macOS region-screenshot mouse capture, native Find Servers Windows/live-connection/input qualification,
intermittent CoreAudio playback startup blocking, Counter-Strike music-cohort acceptance, and the reported Half-Life campaign,
save/load and Leave Game failures. Microphone consent blocking initial loading is an
[explicitly deferred issue](native-migration.md#deferred-microphone-consent-blocks-initial-loading).

A later frozen notify-f0 Half-Life check loaded the protected c1a0c autosave,
accepted movement, returned through actual Leave Game, reloaded through VGUI,
created and loaded a unique owned save, left again and quit with supervised OS
exit0. No no-edicts/nonmember diagnostic was observed in that sequence; this
does not close all campaign/save/Leave Game reports. All eight original saves,
four settings files and289runtime hashes were restored/verified. A distinct
post-exit LaunchServices instance then blocked before Host_Frame in SDL's
CoreAudio playback startup: main waited on readiness while the AudioQueue worker
waited in CoreAudio HAL property IPC. It required guarded forced test cleanup
after normal quit and SIGTERM could not complete. This is not microphone consent,
normal successful shutdown or an audio/performance acceptance. System audio
routes, permissions and other apps were unchanged. Evidence and limits:
`build/macos-runtime-bringup-20261109/hl109-campaign-leave-v1/report.md`.


Subsequent silent native API probes reproduced the current RODECaster83 startup
stall without SDL/game code: both AudioQueue worker and main-runloop-pump cases
blocked at start, and DefaultOutput AudioUnit blocked during construction. A
fresh per-queue Speakers73 control started/stopped/disposed with exit0; the
global default stayed RODECaster83, whose repeat still blocked. Native samples
converge on CoreAudio IOProc creation/stream-usage property IPC. Read-only device
metadata returned promptly with alive1, hogPID-1,48000Hz and512frames. This narrows
the observed playback path, not a proven driver/OS cause or product repair.
Only owned diagnostic processes were cleaned up after bounded deadlines; no
system route, daemon, permission or VM setting changed. Exact evidence and limits
are in the same report/receipt.

The timer report concerned the colon between the digits. Both timer paths now
draw it with the legacy font used for their baseline, through the existing
explicit-font callback. The actual-source fixture passes 44 Debug/Release and
sanitizer case runs; native client owning compile/link checks pass. In the
isolated CS110 colon-only test, the user confirms the corrected colon, normal
FPS and textures in fullscreen. The same process was restarted with the test-only
dummy-audio environment removed, and the user confirms normal game sounds again
through the RODECaster default output. This acceptance uses the earlier reviewed
engine/launcher tuple with the new CS client, not the later music-engine tuple.

An earlier combined music-engine/timer-client test rendered through Apple's CPU
OpenGL renderer with missing surfaces and approximately 4 FPS. Restoring the
earlier engine/client tuple restored the M5 Max renderer; the user confirms the
rendering defects disappeared. The colon-only client also runs on the M5 Max
renderer with normal presentation. The cause of the combined candidate's
renderer selection remains open. These tests still use the OpenGL engine;
the production SDL_GPU Metal backend remains unselected.

The separate windowed HL109 music-v3 test reached gameplay on the M5 Max with the
normal RODECaster output. The user confirms that music is audible and resumes
after switching applications. This closes that bounded Half-Life music focus
check; it does not qualify the separate Counter-Strike fullscreen music cohort.

The user then encountered `MultiSrc:Used by non member DelayedUse.` and a black
screen entering Unforeseen Consequences. The preserved `c1a0c` entity data and
read-only live state identify a blocked teleport master: the one-input
`tele_start_ms` remains off after rejecting the delayed relay, while the player
stays in the dark initial room. The fade has expired, sign-on is complete and
the M5 Max renderer remains selected. The reviewed repair preserves the bounded
legacy final-slot fallback for valid unmatched inputs, including this delayed
relay. Native Release/Debug server links and actual-source sanitizer checks pass;
all four Windows owning compiles and Win32/x64 Release/Debug fixture runs pass.
Live inspection confirms the master turns on and the player reaches the chamber;
the user confirms the chapter is visible and movement works. Evidence is under
`hl109-unforeseen-consequences-black-screen/` in the same build directory.

The subsequent native autosave completed and was loaded into a visible world.
One direct test launch later disappeared at approximately two minutes without a
contemporary crash report. A separate process control exposed inherited process
group cleanup in the diagnostic launcher. Relaunching the unchanged images in a
separate session survives beyond that cutoff; the historical signal sender was
not observed, so this is not a general crash-resolution claim. The earlier music
session completed server/world shutdown and had a voluntary OS exit context,
with an unknown initiating cause. Both observations are preserved under
`hl109-multisource-v1-autosave-exit/`. The loaded save also logs decal restoration
warnings; broader save/decal and transition qualification remains open.

In the separate CS110 `shottrace` session, the user reproduced the trapped region
screenshot cursor. Existing cursor tracing recorded relative mode with native
active/key flags still set while the front window under the pointer was foreign.
The preserved log and reproduction receipt are under
`user-runtime-findings-20261110/` in the same build directory. Overlay timestamps
were not separately recorded, so the trace is an ownership investigation lead,
not a qualified overlay detector. That session ended, its four settings files
were restored and eleven save files were preserved.

The later `keyboard-policy-v1` tests bind the additive Control-Space provider to
current gameplay ownership. HL109 records 45 and CS110 44 actual simultaneous
duck+jump user commands. Fresh-process layout snapshots confirm gameplay
preservation and switching in observed chat, console and menu contexts. Both
supervised runs quit with OS exit 0; Half-Life survives two fullscreen restart
commands. Hardware getters select M5 Max, including CS2560x1440 fullscreen.
Four settings files per app are restored; save contents are preserved. Windows
qualification records 60 owning compiles, 120 preprocessing invocations and
20/20 retained fixture/factory cases; all 860 sealed evidence files rehash.
Detailed native observations are `{hl109,cs110}-keyboard-policy-v1-observations.json`
and Windows results are `gameplay-shortcuts-windows/report.md` in the same build
directory. CUA chords emit both Control keys; these results do not imply new
human mouse, performance or audibility acceptance. One compressed transition
sequence is explicitly inconclusive; isolated observed-context tests qualify
the layout behavior. Screenshot capture remains a separate open repair.

The same Counter-Strike test reveals junk suffixes after developer notify text.
An unmodified complete `Con_DrawNotify` diagnostic reproduces a global-buffer
overread:318 columns at2560px exceed the256-byte message array, while1920px/238
columns completes. Evidence is in `notify-text-diagnostic-v1/receipt.json`; the
repair and real-game visual check remain pending. There is also
a Steam validation-stalled diagnostic, and a CEF signature-category warning
although MOTD HTML renders. These remain separate runtime/provider findings.
The embedded main image fails strict app-resource verification; exact standalone
image bytes verify. A distribution bundle is not qualified by local launch.

Find Servers investigation separately compiles a typed native Steam capability
probe using pinned official headers. Its actual provider requests list CS and HL
servers and receive responses, then cancel/release both requests and exit0.
`server-browser-investigation-v1/runtime-v2-receipt.json` preserves the counts,
time bounds and original loader failure. Queries were intentionally stopped
while still refreshing; this does not qualify complete lists, UI, connections
or authentication. Native UI and lifecycle integration remain unimplemented.

The Mac client build logs, image/source identities and Windows owning-file probe
logs are under `mac-client-build-20261109/`. The current local build receipt is
`build/mac-client-verification-20261109/verification.json`; its evidence is for
compilation and the named integration cases, not native-target promotion.

## Earlier Win32 reference

| Migration | Pinned implementation | Evidence |
| --- | --- | --- |
| SDL2 → SDL3 3.4.20 | `c4f729d`, compared with SDL2 `23a2135` | Four build presets, selected CS/HL R/D recording/full replay, input/focus, startup video, effects and dedicated-server checks |
| Miles music → mpg123 1.33.7 / SDL3 | `af5210a` | Decoder/player/worker checks, captured loop/pause/resume/volume/fade/replacement, selected CS/HL R/D replay and dedicated-server checks |

The SDL3 benchmark compared only those historical packages with matched providers;
its emulated-VM timings are not current-build or native-hardware performance claims.
One mpg123-era CS launch failed with `ucrtbase.dll` exception `0xc0000417`; unchanged
retries passed, but the cause remains unresolved. Exclusive fullscreen visuals,
physical devices/hotplug and complete non-ASCII UI acceptance were not established
by those runs.

Historical receipts remain under `~/.codex/artifacts/cse-sdl3-assessment-20261106/migration/`
and `~/.codex/artifacts/cse-mpg123-20261107/`; Git retains their original reports.

### 2026-11-10: notify bounds and Release frame delivery

Exact f0a6de98 notify repair: native60complete-function cases +30retained
mouse/keyboard/capture cases, coherent native HW/SW/launcher Release/Debug links,
and VM1 Windows8owning compiles/16preprocessing/80cases pass. Original stale-suffix
and sanitizer overread RED retained. Controller rehashes5048source/685native
artifacts and278sealedWindowsfiles with0mismatches. Actual CS1440p/M5Max displays
clean join/scoring text, long notification, short replacement and empty area
after actual Con_ClearNotify. Temporary test-only con_notifytime30 restored4;
developer/FPS/input/audio unchanged. Normal OS exit 0,4settings restored/11saves
preserved. Evidence: build/macos-runtime-bringup-20261109/cs110-notify-bounds-v1-observations.json
and notify-text-windows/report.md. This is bounded notification qualification.

Separate opt-in traced Release CS local listen run: after90seconds,11,121focused
connected/nonmenu/completed frames over142seconds have mean12.783ms,median10.924ms,
p99 24.841ms and496intervals over20ms. Excluding the broad CUA input interval leaves
9,945samples/p99 24.877ms/max37.957ms. Full-sample99.191ms maximum occurs within
that input interval and is not claimed as an unassisted game stall. GPU getter
during warm-up confirms M5Max/1440p then detaches; no compilers/debugger during
selected window. Ordinary background apps/VMs/capture services retained. Graphics
work varies most, but interval n contains previous-frame work plus unmeasured
outer-loop/scheduling: same-row phases do not establish cause. No performance fix,
physical input acceptance or busy multiplayer result follows. Raw CSV/phase/message/
process/analysis: build/macos-runtime-bringup-20261109/cs110-release-frame-v1-analysis.json.
Normal OS exit 0 and settings restored.

Controller follow-up evidence,2026-11-10: `build/macos-runtime-bringup-20261109/native-crash-inventory-v1/` pins eight historical hl IPS reports and two CEF helper reports using actual captureTime/PID/image UUID. No retained hl report establishes the later user-reported disappearance; keep that report open. A CS110 helper SIGTRAP at18:34:51local remains a browser-helper/provider/packaging gate. The inventory induces no crash or signature change. `macos-packaging-investigation-v1/writable-owner-followup-v1.md` records direct relative registry/Steam app-ID writes plus explicit and unqualified VFS writers; no packaging fix is claimed. `network-r5-fixture-investigation-v1/` identifies the existing optimized MSVC fixture noreturn-stub C4702 lead; the original Release gate remains open until a separate reviewed fixture repair and matrix run.

Earlier native server-browser Task1 checkpoint at7bdce560 (superseded by the finalf8d744fc checkpoint below): I1 NUL persistence rejection was repaired; three initial Minor observations were deferred. Real ARM64 model/provider probes for app10/app70 ran8seconds each, copied10000/1534 owned rows and10587/2176 updates with13/44 failed rows, then cancelled/released to quiescence, pumped callbacks after release and shut down Steam; both exited0. No separate successful-response count, complete-list, UI, connection/authentication or live favorites/history-write acceptance is implied. Evidence: `build/macos-runtime-bringup-20261109/server-browser-task1-provider-v1/`. Windows qualification was pending at that earlier checkpoint.

Read-only A3D stock-reference analysis uses the exact VM1 A3dApi.dll SHA256f05b2eeb and Ghidra12.2 complete function bodies. Factory0x10002a80 activates an external OEM/legacy COM provider; the software A2D control body0x10001600..0x1000173a reads only pitch and direct-channel azimuth/elevation/gain/delay, matching retained source inputs. Actual assembly, rather than incomplete x87 pseudocode arguments, supports this mapping. Three PE byte ranges/vtable slots were independently checked; six source files and the exported21-record Evidence bundle are pinned in `build/macos-runtime-bringup-20261109/a3d-reference-analysis-v1/`. No native implementation, output, runtime routing, full DSP/sample parity or OEM EQ/reflection acceptance is claimed. The owned analysis target was closed and the investigation exported/reset; no production source, original assets, VM settings, audio route or permissions changed.

Existing Release frame trace follow-up: correctly lag-aligning interval n with measured work n−1, requiring both rows in focused gameplay after90seconds and excluding145..160seconds of CUA input, leaves9943pairs/446intervals over20ms. Interval/prior-graphics correlation is0.330;227slowpairs retain at least10ms outside the measured prior core, while163have at least15ms of prior measured work. Thus graphics alone is not an established cause. Remainder includes deliberate100fps filtering, host tail, event/policy/music-wrapper work and OS scheduling; none is individually attributed. `cs110-release-frame-v1-lag-analysis.json` retains the paired values and limits. No new runtime, settings change or performance acceptance occurred.

Native server-browser Task1 completed atf8d744fc: reviewed unused model/config plus minimal upstreamformatterpatch/baseline provenance integrated as24exactsourcepaths;453otherdirtyfiles and rootHEADf376 preserved. NativeR/DfullGameUIlinks and Release/Debug/ASan+UBSan components cover both1027-caseformatterbranches; allactualheaderconsumers rebuilt. Final real-modelapp10/70 probes copied10000/1536ownedrows, cancelled/releasedquiescent and shut down withOS exit 0. Windowsactual4newowningcompiles/4fullGameUIlinks/10CTest/28corecases/4108formatterchecks pass underretainedpolicies. Boundedreplay andrecursiveexplicit/DEFAULTLIBclosure are documented; nofreshfullcanonicalrootbuild, loaded-membertrace orliveWindowsUIclaim. Candidate2seals232files(SHAd79ce2a9bc8196d5fd080dadbc8cbc0081e191934179b4d97f186347a8917055), independentlyrehashed0failures and1024originalmetadatarecords unchanged. SourceTask1M1-M3remain deferred, withM1carriedasTask2UIdependency. Task2implementation isactive after23actualsourceRED/controlcases; browserUI/connect/auth/gameplay andwholemigrationacceptance remainopen.


Native server-browser Task2 source is reviewed through54e360fd and integrated as61
exact paths;5,048 outside paths and root HEAD are preserved. Initial/fix1 native
coverage includes75 components,20 real VGUI,5 actual module and5 original CTest
groups per profile. Fix2 newly executes six VGUI plus module deactivate per
Release/Debug/ASan+UBSan profile (21 executions); both full GameUI links contain
one fresh Dialog object and138 individually proven reused objects. Unchanged
suite evidence is inherited with exact input/recipe/object proof. Both scoped
reviews close their original Important findings; deferred count/sort feedback and warning
debt remain for parent review. The independent final controller audit checks
561,548 identities/conditions with zero failures. These are source/component and
owning-build results, not complete-game or Windows acceptance.

At that same final model, actual Steam app10/app70 Internet discovery runs8seconds
each and copies10,000/1,533 rows. Full CloseStep(128) storage retirement takes329/59
incomplete turns, followed by provider quiescence, post-release callback pumps and
Steam shutdown; both processes exit0. Four typed read-only snapshot processes also
exit0; all669 favorite/history records remain logically unchanged around both
phases. No Steam setter, connection, audio or game runtime action occurred. Exact
private snapshots, source/provider pins and limits are sealed in
`build/macos-runtime-bringup-20261109/server-browser-task2-provider-v1/`.
The 54e360fd candidate was subsequently staged into both owned game apps. Actual
Counter-Strike keyboard menu activation opens Servers and populates Internet rows
on the M5 Max at 1600x1200. The inherited HD font context makes the browser's fixed
controls unreadable and overlaps tabs with the list header (I5). Native App-menu
Quit exits 1; guarded reproduction and exact-image disassembly identify an absent
embedded-root fallback in CInputWin32::UpdateMouseFocus (I6). An initial debugger
watch incorrectly continued unexpected fault stops until its deadline; that
instrumentation failure is preserved separately from the actual crash evidence.
Those original exit1 runs are not normal-exit acceptance. Settings, saves and browser preferences were
restored, all 669 Steam records remained equal, and 289 runtime files plus five
symlinks were verified after each phase.

Round 3 candidate 90548ca passes 96 selected R/D/ASan+UBSan cases and independent
controller audit 373,100 checks with zero failures. Scoped review accepts I6's
native64 guard but retains I5 focus after resize/nested focus and raises unstable
tab order from mutable z-order. A fresh round 4 implementer owns those browser-local
repairs; 905 layout is not integrated or staged.

Only reviewed 905 InputWin32.cpp and its new VGUI2 image were subsequently admitted
with existing 54e GameUI/engine/main. Exact 38-TU Release/Debug dependency proof shows
InputWin32 as the sole changed production input; root HEAD/index and 5,108 outside
files remain unchanged. Actual CS 63458 and HL 63579 open Servers and complete native
App-menu Quit with supervised OS exit 0. Half-Life visibly populates Internet rows,
and lsof resolves its staged VGUI2/GameUI. No debugger/forced cleanup is used.
Each phase restores four settings/all original saves (11 CS/8 HL)/browser preferences,
all 669 Steam records remain equal, 289 runtime files/five symlinks verify and no
owned process remains. Deliberate process-local dummy audio and potentially active
layout compilers exclude audio/physical/performance acceptance. Evidence is sealed
in `build/macos-runtime-bringup-20261109/server-browser-i6-only-stage-v1/`.
At that I6-only checkpoint, combined-candidate checks remain required; it closes
only the bounded native menu-quit repro in the explicitly composed cohort.

Round 4 e7eb139 addresses retained/nested focus and stable logical tab order, with
fresh scoped approval and no open findings. Its 45 selected R/D/sanitizer cases and
two binding runs pass; independent controller audit 369,707 checks/26,478 files has
zero mismatches. All 67 Task2 paths are integrated into root (10 changed relative
to the approved 54e+I6 composition), preserving 5,047 outside files, HEAD and index.
Only new GameUI is staged; the already reviewed VGUI2 remains byte-identical.

Actual CS 67614 and HL 69257 show readable 1600x1200 browser controls, separated tabs/
headers and Internet rows. Keyboard traversal reveals lower controls; Half-Life's
Quick Refresh starts a visible refresh. Both native App-menu quits return OS exit 0.
Each phase restores four settings/all original saves/browser preferences; 669 Steam
records stay equal, 289 runtime files/five symlinks verify and no owned process
remains. No debugger or forced cleanup is used. These phases use deliberate dummy
audio and do not qualify audio/performance/physical input. CUA coordinate clicks
retain the earlier physical-pointer limitation; Escape is inherited nonmodal Frame
behavior and CtrlF4 did not visibly close. Browser close/reopen and live tab/resize
actuation are not claimed from these phases; actual VGUI source regressions remain
separate. Records: `server-browser-live-cs110-fix4-v1/`,
`server-browser-live-hl109-fix4-v1/` and
`server-browser-task2-fix4-integration-stage-v1/` under the runtime evidence root.

VM1's console and existing read-only source share work despite bounded host
guest-exec timeouts. An ordinary-user collector verifies12 tool files,78 fixture
files and2,003 preserved inputs with no identity/stability mismatch; disk floors
pass. Both running fixture image/argument queries are denied and remain unknown.
Primary game ports are present; port27440 is also observed, consistent with the
historical -sport argument but outside the old primary-port readiness reference.
The expanded Windows input-owner closure and final source/command admission remain
pending. No VM configuration, security, audio route or global permission changes
were made.
Task2 Windows owning/reference and the remaining full connection/input/runtime
gates remain open. The separate CoreAudio startup failure, terminal Atlas/TLV recovery,
physical/performance/authentication and whole-migration gates are unchanged.
