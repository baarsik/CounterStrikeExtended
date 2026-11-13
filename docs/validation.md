# Validation status

**No native target is fully qualified.** Results below belong to frozen inputs;
later source edits require fresh evidence. Windows VM1 runs Windows 11 ARM64 with
x86/x64 emulation. Mac runs use Apple Silicon and a macOS 14 deployment target,
not macOS 14 hardware acceptance. [Requirements and open gates](native-migration.md)
and [reproduction](../tests/native/README.md) remain separate from build results.

The later November 13 six-finding static-review follow-up changes final Windows
fast-mutex release ordering, macOS microphone channel/source control compatibility,
and native browser game-directory matching, keyboard viewport/selection handling
and secondary sorting. These changes received source/diff review only, under the
user's no-testing instruction. No tests, compiler probes, builds, game/UI launches
or device checks were run for this follow-up. Earlier receipts below apply to
their frozen inputs and do not qualify the resulting source. The spectator
micro-flick still has no established cause.

The November 13 seven-finding parity follow-up restores native VGUI1 font
weight/style/width/rotation requests and ordinary missing-name fallback; native
HTML dirty upload bounds across paint acknowledgements, including the standalone
backing-image stride; fullscreen FBO `-freq` selection at desktop dimensions;
signed browser-position persistence; and VGUI1 event delivery for Half-Life's
MOTD and other legacy controls. Native A3D retains eight rendered voices while
excess managed sources advance logically, and exposes the retained software
occlusion attenuation through explicit Enable. Existing font ABC/cropping,
default feature activation, input eligibility and old browser config defaults
remain covered.

Focused Release/Debug/ASan+UBSan results per profile: font/FixedSys 4/4,
A3D resources 29/29 and spatial 10/10, HTML 6/6, fullscreen 8/8, complete SDL
pumps 2/2, browser config/model 5/5 and six actual-dialog geometry cases.
The Windows min/max macro regression has a separate actual-source compile
RED/green and config check, 2/2 per profile. Twelve affected owning TU
compilations and both complete macOS ARM64 runtime builds pass. No game or
physical testing was performed; dialog/provider checks use dummy services.

Native font matching/synthesis is functional compatibility, not certified GDI
pixel/metric identity. Virtual A3D uses the retained fixed 11 bytes/ms timing
rule at the native source boundary; exact old timing-ring/prefetch latency and
OEM acoustics remain unqualified. HTML still copies/transports full backing
images. Windows/MSVC builds were not run. AppleClang LeakSanitizer is unsupported;
supported ASan/UBSan checks pass with leak detection disabled. The pre-existing
broad shortcut lifecycle fixture still has stale launcher/listen seams; focused
complete-pump tests pass. The spectator micro-flick remains unresolved.

The November 13 Half-Life scoreboard follow-up restores retained native font
atlases across font switches. After three-font warmup (768 glyph rasters and
three page uploads), 72 component selections/draws perform zero rasterizations,
texture creations/uploads/retirements or allocations. Cache hits also acquire
no extra module references. Per-use preparation, catalog/renderer rebuilding,
partial retirement and failed-release retry remain covered. Atlas suites pass
Release 10/10, Debug 10/10 and ASan/UBSan 9/9; full ARM64 runtime builds pass in
both profiles. This is component/build evidence; game FPS remains a manual check.

The November 13 kill-feed follow-up centers weapon and headshot sprites within
console text lines that are taller than the sprites. The retained source and
shipped x86 Mac client both used top alignment, so this corrects an inherited
high-resolution layout defect. Text and normal-size sprite positions are
preserved. The complete production draw-body fixture passes 16 cases in each
Release, Debug and ASan/UBSan profile; both full ARM64 runtime builds pass.
No game was launched and visual acceptance remains a manual check.

The November 13 manual-test follow-up preserves client mouse activation requests
across native capture suspension, including recovery while a menu remains open.
Ordinary deactivation still cancels the request. The actual owner, client wrappers
and both UI cursor callers pass three component cases in each Release, Debug and
ASan/UBSan profile. Related mouse transaction/lifecycle cases pass 3/3 and shortcut
event loops 2/2. The broader shortcut lifecycle fixture still has stale seams;
its compile failure also reproduces on the unchanged preceding source.

The recorded Half-Life multiplayer dump identifies an intentional native VGUI1
font-selection abort. A reproduced upload-boundary defect is corrected: prior
shared-context GL errors no longer reject an otherwise successful font upload;
errors from that upload still fail. Atlas component suites pass Release 10/10,
Debug 10/10 and ASan/UBSan 9/9. The dump does not retain the failed result, so the
connection between that defect and the exact reported abort remains an inference.
Both complete ARM64 runtime build profiles pass; no game was launched by the
agent. The reported spectator micro-flick remains unresolved. Camera behavior
and defaults are unchanged.

The November 13 six-finding review fixes restore native module publication order
and case-insensitive instance lookup, retain unreachable saved browser endpoints,
preserve whole-refill A3D interpolation, propagate output failure/recovery through
managed buffers and public sources, and repair native HTML scrollbar geometry and
resize ordering. Fresh macOS ARM64 Release/Debug `cse_runtime` builds complete for
all 19 roles. No game was launched for these fixes.

Focused checks: system modules 42/42 ordinary cases; HTML 13/13 ordinary and
ASan/UBSan cases; browser five CTests plus three expected-abort controls in each
Release/Debug/ASan+UBSan profile; A3D output 33/33, buffers 31/31 and resources
27/27 in each profile. Supplemental system-module sanitizers pass 44/45: the
enabled `lifecycle_image_reload` case retains the documented AppleClang 21 globals
registration failure, freshly reproduced by the project-independent reload
fixture. This is component/dummy-output and build evidence, not physical output,
gameplay or complete native-target acceptance.

## Evidence by boundary

| Boundary/cohort | Bounded result | Limit / next gate |
| --- | --- | --- |
| Windows V18 Win32/x64 R/D | All 19 builds; architecture/import/export/resource checks. Native Debug resumed after disk reserve with identical inputs | Historical cohort; fresh combined build after later edits |
| Windows V17 x64 CS/HL R/D | Four recording/full-replay cases, 651 frames and 15 loaded-image pins each | Selected flows; complete lifecycle/presentation/voice/numerics open |
| Windows V17 external server R/D | Authorization, AMXX menus/HUD/messages/cvars, downloads, consistency and alive movement | Unchanged ReHLDS 3.15.0.896/Metamod-R 1.3.0.149/AMXX 1.9.0.5303; no voice/audibility or live VAC-secure-bit claim |
| Windows V11 data/lifecycle | Eight Win32↔x64 demo legs, four HL save legs, selected reconnect/changelevel | Full entities/transitions/C4/navigation and Mac exchange open |
| Windows V18 camera R/D | 1041 alive frames, 199 movement-command frames, 410 exact angle samples each; Debug 18 state samples; multiplayer third-person rejection | Threaded single-player camera scope only |
| Mac `6f27bef` full 19 R/D | Normal `cse_runtime`, 38 images containing ARM64 per profile; selected CS map/reload/quit and controlled HL archive marker on M5 Max OpenGL | Historical runtime checks; current combined builds recorded separately |
| Native A3D `9498efa` R/D | Reviewed enumeration/selection; 18 dummy cases/54 lifetimes; four real Speakers cases/12 lifetimes, separate default-selection and DMA/music coexistence | PCM submission is not acoustics, RODE startup repair, physical removal, OEM or actual-game routing |
| Save archive `25fb58a` + `6f27bef` HL | MSVC Win32/x64 R/D: eight in-memory cases; actual 260-byte entries and armor 0→15→0→15 plus weapon-mask restore | Same-build player marker only; entity-table counts 336→271 do not prove entity/callback parity; no cross-platform/campaign acceptance |
| Mac actual-library UI R/D | Selected clock/args/date/disk, clipboard reads, keys, fonts/borders/popup bitmaps and lease teardown | Dummy video/software renderer; physical input/pasteboard publication/default layout open |
| Mac historical user checks, November 10 | Mouse/fast turns/wheel bhops, no pointer, CS 1080→1440 fullscreen, timer colon/effects; HL music focus/resume and repaired Unforeseen Consequences observed | Original cohorts only; no acceptance transfer to new combined images |
| Input/capture/notify repairs | Reviewed source and owning R/D/Windows/component checks; observed gameplay/UI shortcut ownership, native capture overlay release/reacquire, bounded 1440p notify rendering | Physical region drag remains unqualified; no broad input/HUD/performance claim |
| Private SDL_GPU components | Three selected D3D12 profiles and one Metal profile match 12,288 frozen GL RGB bytes; only row order normalized | Offscreen/private profiles; production backend unselected; other features/driver reference failures retained |
| Legacy dispatch T2a `cd36b23` | Windows x64 D/R 62 and Mac ARM64 D/R/sanitizer 93 cases; six-TU M5 Max GL2.1 comparison matches exact RGBA/state/query outputs, zero errors and traces across 45 entries, including forwarding after deferred-clear/vertex-limit refusal | Restricted gl_rmain/r_triangle/glHud profile; CaptureOnly stays fixture-only; full producers/startup, deferred capture, T3 barriers and game acceptance open |
| Standalone VGUI resize `cce153` | 56 R/D/sanitizer component cases, original no-drain Add Server/eight ready-window dummy cases and four controlled Cocoa window-API cases pass; Add Server 471 height / 14 query ports retained | Software-widget/window-API evidence only; live Steam, pixels/visuals, physical input, game/GPU/audio and full migration open |
| Extended capture T2b17 | Frozen Windows 90 and Mac 135 D/R/sanitizer cases pass; six real-GL M5 Max cases match pixels/state/errors/arguments; scoped result review `0f40cbfc` PASS | Palette unavailable explicitly unqualified; no production Metal/D3D12 or complete-game claim |
| Combined Mac all19-002 | Both actual `cse_runtime` Release/Debug commands exit 0; 19 roles, 307 files, five links and 38 images containing ARM64 per profile; independent specification/quality review PASS | Incremental build/image inventories only; no game execution, complete ABI/provider qualification or native acceptance |
| Original browser Tasks1/2 | Reviewed original-resource/model/secondary source and CPU/dummy-widget cases; frozen Mac Debug GameUI 139 TUs plus 62 CPU checks / 154 recorded commands pass, independently reviewed `a88d895a` | Source/component evidence; Windows fix7 owning/CPU slice recorded separately |
| Windows browser fix7 | Final actual PASS: x64 Debug/Release 137 owning GameUI TUs and 62 ordinary cases each; x86 Debug/Release 130 TUs and 53 cases each; 230 total, 72 commands exit 0; independent specification/evidence-quality review PASS within scope | Build/link/CPU only; archived 229-case progress is interim, final external receipt is authoritative; no visual/provider/gameplay or full Task3 acceptance |
| Final GameInfo player-generation repair, November 12 | Mac Release/Debug: changed owning GameUI TU rebuilt and full module linked with 138 unchanged reviewed owner objects per profile; 20 actual-dialog dummy cases across ordinary and ASan/UBSan R/D, plus 14 focused details-owner controls pass | Focused build/component evidence on the repaired sources; dependency images remain uninstrumented, Windows was not rerun, and Task 3/runtime acceptance remains incomplete |
| Windows browser failed fix5/fix6 | fix5 stopped at x86 SDL configure after 124 x64 CPU passes; fix6 retained the x86 signed/unsigned rank-fixture compile failure | Immutable failed packets remain historical; fix7 does not rewrite them |
| Original browser actual 012 | Actual native frame/all 6 tabs and selected History/Spectate rows; scoped lifecycle/protection evidence reviewed | Original quality FAIL and parked prose-hash finding remain; no expanded interaction or full-fidelity qualification |
| Original browser actual 021 | Partial Internet-open observation accepted; original120 tab-observation phase expired; recorded retirement/app/config/CF3/audio/TIS restoration reviewed | Runtime remains failed/unaccepted; no all-tabs or interaction qualification; later CS026/HL027 remain separate failed cases |
| Original browser CS026 | FAIL/unqualified: submitted open record omitted required `browser_visible:true`; partial actual Servers/Internet-opening evidence and bounded closure reviewed | No repaired field or accepted open/tabs/filters/quit coverage; CmdQ cleanup is not typed-quit protocol; opaque Steam drift remains |
| Original browser HL027 | Consumed/unqualified: specification/procedure FAIL; accepted open and Internet rows only; independent closure-audit quality PASS within restoration/opaque-Steam-drift limits | First click already showed ambiguity; later second click and held Ctrl+Tab violated stop rule; wrong stage-path-string hash stop was correctly refused; no tabs/filter/query/list/resize/focus/secondary/close-reopen/typed-quit or full-browser qualification |
| Original browser CS031 | Full-browser specification FAIL / strict procedure FAIL; independently reviewed narrow functional PARTIAL PASS with documented held-shortcut discriminator deviation: five groups, 32 ordinary actions/32 images through open/tabs/filters/query/list; bounded evidence/closure quality PASS with opaque Steam drift | Six tabs, Map d+Backspace, one checkbox on/off, expand/collapse, Refresh/Stop and Internet/History list only; latency/security choices/authentication inconclusive; edge drag selected a row, then correct-hash stop/safe CmdQ; no resize/focus/secondary/context/close-reopen/typed-quit or full Task3/runtime qualification |
| HL fresh 017–020 | 017 paste field stayed empty/no Return; 019 operator120 timeout; actual020 failed with missing map-Return protocol receipt despite Root-reported Return/world; retirement/restoration reviewed | No own-save/death/reload acceptance; preserve original failed receipts |
| HL actual023 | Original signon-display/helper-retirement gates failed; later Root quiescence and app recovery reviewed | Original retirement flag remains false; raw after display unbound; no runtime/save/death acceptance |
| HL actual024 | Echo/map Return and first-world observations recorded; initial-status console phase expired at original120; retirement/app/CF3/audio/TIS/display restoration reviewed | Runtime remains failed; no save/death/reload or accepted quit; retained sample text does not prove sample exit 0 |
| HL actual025 | Consumed at prelaunch only: actual two 144-Hz displays mismatched prospective one 1080p60 display; owned controller aborted through normal cleanup; no game PID or execution | Restoration reviewed; raw prelaunch/after equality does not qualify the unbound display contract; actual028 failure is recorded separately |
| HL actual028 | Runtime FAIL/unaccepted: genuine n28 native save and recorded kill/death/one Space named reload to c1a0/server2; duplicate/foreign quick activity and unresolved post-reload UI timeout retained; bounded closure/recovery audit PASS with external drift | No accepted final WORLD/movement/typed quit or entity/callback/campaign qualification; general outside interaction does not attribute individual inputs; later separate028/CS021 revival recovery preserves user outputs and never repairs original failed receipts |
| HL actual029 | Narrow assisted functional PASS: genuine native n29 save, one kill/death, one Space/named reload, current-world movement/status and normal quit; independent bound-evidence/closure review and Root scope ruling | STRICT PROCEDURE FAIL: Root continued after no-focus trace/no-response instead of stopping, then used one neutral human activation click; original accepted=false unchanged; no strict unattended, full entity/callback/campaign or native-target acceptance |

The accepted T2a green-2 source archive contains 5,300 files, SHA-256
`0a008c890329a1511fd31d03f3a932b26a54056f2e3b27d8d3301521af2d2d55`.
Each T2a profile includes 10 dispatch plus 21 retained frame/state/texture cases:
CaptureOnly query refusal within Begin/End, unchanged outputs, sticky failure
and Abort recovery. Production continues to select ReferenceGL. Resize probes
retired normally and restored CF3 exactly; their software rendering remains a
separate boundary from production OpenGL.

Combined all19-002 binds 5,306 source files to source ZIP SHA-256
`b2d4c60d3d7a3df88a8634623b02c7c69cf5a9a17069f49a1d94e89b9b9d064d`;
its receipt SHA-256 is
`ab022374eeeea32bd65fdee46ddce9bae7106ccbe885580d4171b70d78c34588`.
The intermediate source ZIP
`d7742ac578b02fa038a6ff26fc620b4ed55d5124e7ce4380ff7fdf45f708b811`
changes three Windows browser fixture files. Final Windows fix7 source ZIP
`e93a11cf75c6208cd24c84e905e5ad979c644d911c6c58805ad14e65e34dc9ba`
contains 5,306 files and adds only the presentation rank-fixture type correction
to that intermediate snapshot. All four deltas from all19-002 are fixture files;
production/Mac inputs are unchanged between those build snapshots. Before the
final player-generation repair, PUB production sources and tests except
`tests/native/README.md` matched fix7 byte for byte. The reviewed documentation
cleanup deleted 26 files and replaced four, reducing the manifest from 5,306 to
5,280 members; it did not establish whole-manifest equality. The later
player-generation source/test repair has the separate focused proof in the table
above. Windows fix7 and all19-002 retain their historical source boundaries; no
runtime outcome is transferred.

A scoped MSVC-only C6101 push/pop around `ws2tcpip.h` in
`ServerBrowserDetails.cpp` addresses a pinned SDK 10.0.26100.0 unused-inline
analysis diagnostic. Final fix7 actual commands preserve owning per-source
policies, /W4 /WX, CRT and /analyze bounds; its independent review passes the
four owning builds and 230 ordinary CPU cases. Final external receipt SHA-256
`57815c0d6f2b0c43d7fafd090bc19626dcb4cd4c40d93a478efa649e59b32935`
binds immutable result ZIP
`0a5889851695e03dc48a6eacce3dc6808445166602d321f00fac0796a37d868d`.
The archived progress snapshot remains interim FAILED/229 cases/three completed
profiles; the final receipt records all four profiles and 230 passes. Do not
rewrite that interim snapshot or infer visual/provider/game acceptance.
Preserve failed fix5 archive
`0edd92bd24aeda872185bdbfb35bd5e417680ea46e3b197e3f18eb71b8170b43`
and the separate fix6 failure. Actual029 adds only the separately reviewed assisted
functional sequence; strict activation-guide compliance remains FAIL. Root continued
after inactive/no-response evidence, then requested one neutral human activation
click; later commands stayed under original clocks and ownership checks. This is
not human gameplay acceptance or retrospective permission to waive the guide.
CS026 remains failed for missing `browser_visible:true`. HL027 now has final
specification/procedure FAIL and bounded closure-audit quality PASS; its first
ambiguous click was already a stop condition, before the second click/Ctrl+Tab.
CS031 final independent review records full-browser specification FAIL and strict
procedure FAIL: the prospective Root discriminator ruling did not satisfy the
original stop-on-ambiguity guide. Five groups/32 actions/32 images support narrow
functional PARTIAL PASS with that deviation; only one checkbox was toggled.
Default-duration tab input did not switch tabs; held0.2-second Ctrl+bracket input
did, without proving the initial no-response cause. Latency/security dropdowns
and authentication remain inconclusive. Edge drag selected a row rather than
resizing; Root correctly stopped and used safe CmdQ, not typed quit. Bounded
closure/evidence quality PASS includes opaque Steam drift; pointer/resize/focus,
secondary/context/close-reopen/typed-quit and full Task3/runtime remain unqualified.
Full runtime/Task3/campaign qualification remains pending; failed028 and assisted029
retain distinct results and procedural/protection limits.

## Open runtime gates

**INCOMPLETE / UNQUALIFIED; deferred for publication only by explicit user
instruction:** “Ignore those validations, mark them incomplete and proceed as is.
After you commit and push, let me know what I need to test manually and give me a
link to the app”. This removes the missing runtime validations as publication
prerequisites; it does not complete Task 3, repair failed receipts, transfer
acceptance, or qualify the full native migration. The publication handoff must
include the remaining manual checks and a link to the delivered app candidate.

- Complete actual combined-cohort runtime checks. Preserve Windows fix7 as
  four-profile owning/230-case build/CPU evidence and all19-002 as Mac incremental
  build/image evidence; full module ABI, exports/imports/resources, generated
  policy and provider acceptance remain separate gates.
- Complete original-browser filters/resize/selection/context/secondary dialogs,
  close/reopen, connection/gameplay/disconnect and external authentication.
- Complete HL campaign/entity/callback/save/death/transition tests and bidirectional
  native/updated-Win32 save/demo/navigation exchange. Later successful bounded
  Leave Game/player-marker sequences do not close the broader historical reports.
- Qualify physical mouse/fullscreen region capture, focus, text/IME/layout, normal
  audio/capture/voice, default RODE startup, full spatial/OEM/game routing and
  dedicated/admin/HLTV/software behavior on the combined cohort.
- Finish production renderer work and measured presentation/pacing; keep OpenGL
  production/default until feature, scene, lifetime and display gates pass.
- Complete provider failure/restart, CEF helper identity/signing and writable-data
  packaging. HTML rendering alone does not qualify every helper. Relative
  `hl.conf`/`steam_appid.txt` and VFS config/save/download writes remain packaging
  obligations. The historical CEF helper SIGTRAP remains unresolved.
- Preserve the [deferred microphone-consent startup issue](native-migration.md#deferred-microphone-consent-blocks-initial-loading).
  Playback HAL blocking and microphone consent are different findings.

Historical uneven Release delivery measured 11,121 focused frames over 142 seconds,
median 10.924 ms / p99 24.841 ms after 90-second warm-up. Lag-aligned analysis does not
establish graphics as sole cause; intentional 100-FPS filtering (`fps_override 0`),
outer-loop work and OS scheduling remain contributors to investigate. The old
combined candidate that selected Apple's CPU GL renderer is a retained regression,
not current hardware acceptance. Verify actual GPU selection on each new cohort.

## Evidence locations and interpretation

Exact source/tool/command/object/image manifests, original failures and reports
are external evidence, not shipped runtime files. Preserve failed attempts and
private data; do not overwrite a failed wrapper with a later recovery result.

| Evidence root | Useful receipts |
| --- | --- |
| `~/.codex/artifacts/cse-native64-20261108/` | `runtime-platform-v18-*`, `native-camera-production-v18-final/`, `hl-ui-lengths-v1/`, `render-metal-v1/`, `xcode-metal-component-v1/`, `macos-runtime-build-native-audit-v19-*` |
| `build/macos-runtime-bringup-20261109/` | Original gameplay/input/notify/music/multisource, browser provider/Steam snapshots, frame trace, CoreAudio/crash/packaging findings and immutable source/image packets |
| `build/a3d-native-20261111/` | `task8-controller-audit.json` full 19 build proof |
| `build/native-selected-device-live-v1/` | `root-live-subgate-summary.json`; output/default-separated and coexistence cases |
| `build/native-client-6f27bef-qualification-v3/cs/` | `root-post-lifecycle-review.json`; failed stale-CF wrapper and separate `root-cf-recovery-001/receipt.json` retained |
| `build/native-client-6f27bef-qualification-v4/hl/` | `root-save-marker-review.json`; corrected CF3/audio restore and 109.21-second normal exit |
| `build/production-render-dispatch-2a-20261111/` | `green-2/root-actual-windows-green-gate.json`, `mac-qualification-green2/receipt.json`, `real-gl-root-output-001/receipt.json` and scoped review; GL receipt SHA-256 `6c07f9c82d3c005e38f134b5c9578e4fda5b283be61af6d9f69fdcaf5614ce04` |
| `build/native-vgui-resize-runtime-candidate-v3/` | `root-green-gate.json`; replay and controlled Cocoa gates in sibling `native-vgui-browser-replay-v1/root-result.json` and `native-vgui-browser-cocoa-v1/root-cocoa-gate.json` |
| `build/production-render-dispatch-2b-20261112/` | Frozen Windows/Mac/real-GL packets, failed controls and independent review |
| `build/original-browser-task3-build-qualification-v2-mac-actual-001/` | Current frozen Mac Debug owning/component result |
| PUB `build/final-publisher-integration-20261112/all19-002/` | Actual `receipt.json`, source binding, both runtime inventories and `independent-actual-review.json`; build/image inventory only |
| `build/original-browser-task3-build-qualification-v1-windows-fix7/actual-review-001/` | Final `actual-review.md/json`, evidence pins and authoritative fix7 publication receipt/result ZIP; prior fix5/fix6 failures retained separately |
| `build/native-hl-native-own-save-death-runtime-preparation-v7/`, `-v8/`, `-v9/`, `-v11/` | Actual020/023/024/025 sibling `root-actualNNN-closure-review-001/report.md/json`; failed or prelaunch-only cases remain unaccepted |
| `build/original-browser-task3-remaining-native-preparation-v2/CS021/` | Actual021 partial Internet-open/tab-timeout closure; later attempts retain separate evidence |
| `build/native-hl-native-own-save-death-runtime-preparation-v12/root-actual028-closure-review-001/` | Actual028 failed partial save/death/reload, unresolved UI/extra quick activity and bounded closure/recovery audit; no accepted final WORLD/quit |
| `build/post-user-interference-recovery-20261112-plan-001/root-actual-001/` | Separate `receipt-028.json` and `receipt-021.json`: later app revivals recovered, user outputs preserved twice and six physical trees independently checked; old qualification unchanged |
| `build/native-hl-native-own-save-death-runtime-preparation-v13/root-actual029-review-001/` and `build/native029-functional-scope-ruling-20261112.json` | Actual029 assisted functional PASS, strict procedure FAIL, bounded closure and Root acceptance-scope ruling; original receipt/guide unchanged |
| `build/original-browser-task3-remaining-native-preparation-v4/CS026/root-actual026-closure-review-001/` | Failed missing-`browser_visible:true` open record and bounded closure; no accepted tab/interaction coverage |
| `build/original-browser-task3-remaining-native-preparation-v4/HL027/root-actual027-closure-review-001/` | Final spec/procedure FAIL, first-ambiguity continuation and wrong stage-path hash; closure quality PASS within recorded restoration/opaque-Steam-drift limits; original failures unchanged |
| `build/original-browser-task3-remaining-cs031-preparation-v1/CS031/root-actual031-closure-review-001/` | Final `report.md/json` and `pins.json`: full-browser/strict-procedure FAIL, narrow functional PARTIAL PASS with discriminator deviation, bounded evidence/closure PASS; retained actual records and failed receipt remain unchanged |

Native 012 lifecycle/protection and honest-coverage reviews pass within their
recorded bounds. All 669 Steam favorites/history records match; previous CEF
files are unchanged and new files belong to the owned namespace. Opaque Steam
localconfig content drift remains unresolved and is not field-level causality
proof. No opaque Steam restore was performed. A nonoperative prose-hash typo in
the external 012 handoff remains a parked quality finding with an explicit Root
ruling; do not relabel the original quality FAIL. Actual021 adds only a partial
Internet-open observation before its tab timeout, with no interaction qualification.
Actual020/023/024/025 closure reports distinguish recorded retirement/restoration
from failed or unexecuted runtime cases. Each retains unresolved opaque Steam
byte drift. For023, later recovery does not rewrite the original failed helper
retirement flag; for025, raw display equality does not repair missing accepted
bindings. Earlier CS CF restoration failure/recovery and later HL exact-domain
success likewise remain distinct.

Actual028 retains its failed receipt despite genuine n28 save/death/Space reload.
Later unresolved UI tool timeouts and extra quick-save/load activity do not prove
which individual events came from the generally acknowledged user interaction.
Separately started028/CS021 apps were recovered under separate receipts; both
private bundles match baseline and each user-output preservation/exchanged tree
matches its retained state (six trees independently reviewed). Original failures
remain unchanged. SDK669 records match, while prior CEF drift and opaque Steam
hash drift remain; no per-event attribution or opaque restore is inferred.
Historical display equality is not a final post-interference quiet proof. Recovery
PASS is bounded restoration evidence, not clean full runtime protection.

## Earlier Win32 reference

SDL3 `c4f729d` versus SDL2 `23a2135` qualified four historical presets and selected
CS/HL R/D record/replay, startup video/effects/input/focus/dedicated paths. mpg123
`af5210a` qualified decoder/player/worker and selected capture/replay/server paths.
Their receipts remain under `~/.codex/artifacts/cse-sdl3-assessment-20261106/migration/`
and `~/.codex/artifacts/cse-mpg123-20261107/`; Git retains original reports.
One mpg123-era CS `ucrtbase.dll` exception 0xc0000417 remains unexplained despite
passing unchanged retries. Exclusive fullscreen, hotplug, complete non-ASCII UI
and native-target acceptance do not follow from those old emulated-VM checks.
