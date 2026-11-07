# SDL3 migration validation

## Qualified SDL2 reference

Baseline source repair: `a452d39` (TGA expression grouping); demo-validation
correction: `23a2135`. Qualification tools: `c99ca14` with subsequent transport fixes.
The assessment tree and migration base have identical tree ID
`8ff46febc8d3ab7c3327fe66b34073827c0c203c` despite different commit histories.

VM1 is Parallels `Windows 11`, UUID `dc54534b-5d1e-4bae-816a-a60b8038b1c6`,
Windows ARM64 build 26300 with 12 vCPUs and 20 GB RAM. Windows x86 binaries use
VS 2026 MSVC v145 (19.51.36260.0, ARM64-hosted x86 compiler), SDK 10.0.26100.0
and CMake 4.4.4. Both hardware and full runtime Release/Debug presets passed.
The affected full runtime Release/Debug builds were repeated at `23a2135`.
No warnings, assertions, provider checks or floating-point options were relaxed.

`LogoFile.cpp` previously failed C6336/C2220. Explicit parentheses preserve
its original ternary evaluation; multiplying by the ternary result would change
behavior and was deliberately avoided. The suspected sign-on defect was a test
interpretation error: this engine uses `SIGNONS = 2`. Default single-player CS
training rules auto-assign the player; a subsequent scripted team join killed it.
The multiplayer fixture uses `-maxplayers 2`. Fixed window size additionally
requires `-forceres`; unsupported display enumeration otherwise selects a fallback.

Both games ran from isolated data overlays with all 16 source-built runtime
outputs verified by SHA-256, licensed Steam providers, `-gl -windowed -forceres
-w 640 -h 480 -maxplayers 2`, vsync disabled and localhost multiplayer maps.
Each recorded 651 live-player network samples over about 6.52 seconds, including
301 movement-command samples and changing simulated origin. CS used `de_dust2`;
Half-Life used `crossfire`. The native x86 collector sampled loaded module paths
and hashes while the game was running. Source-built loaded modules matched the
staged runtime manifests. The SDL2 provider was 2.0.20.

The persisted probe reached `Host_EndGame: End of demo.` before clean exit zero:
CS reported 666 host frames and HL 654, compared with 651 recorded network frames.
Host-frame and network-frame counts are distinct. Earlier runs stopped playback
at 499 frames and are excluded from benchmark evidence. Internal renderer TGA
captures show textured worlds and HUDs at 640×480. Parallels CLI captures show
black accelerated regions even for Steam; those captures cannot assess rendering.

Compact machine-readable identities are in `sdl3-baseline-evidence.json`.
Full logs, demos, hashes, native module inventories, TGA captures and build logs
are retained outside the source tree at
`/Users/mmalakhouski/.codex/artifacts/cse-sdl3-assessment-20261106/migration`.
The two accepted fixed benchmark recordings in `baseline-assets/` have hashes:

- CS: `fb1a18edf31182d49b022a46850df0cd263fe1ea101b28caac78af038fdf211c`
- HL: `3177530c2d13a5e88f2ffc4f86f0baefe939034cde0b5c8d787583f4c035ffa3`

The baseline source-built Release runtime is preserved on VM1 at
`C:\CI\CSE-SDL3-Migration-20261106\baseline-runtime-release`; qualified game
sandboxes are separate from future SDL3 sandboxes. Reproduce a recording/replay
using `tools/qualification/README.md`. This baseline establishes those exercised
paths; final migration qualification must separately cover input, display, media,
optional builds and complete dependency removal.

## Final product and build provenance

The final product includes cursor-ownership fix `cf7e58c`. The fresh Windows source
archive is commit `c4f729df40a33a3eec7458a3b2ccaac463bd8882`, tree
`942975c6fcf89600e4807d00a838c9ae2a298d51`, ZIP SHA-256
`8e827de5d43bed6ae7851f9004d0db175839a12679be5575b1f1a2f5e8fbec53`.
`E/final2-product-source-manifest.json` records its full identity. Its production
SDK subtree is `8cb90868bd5e6e95026dbc0a2fa4c4cd983554d5`; CMake subtree is
`e9623cb1b61a0254c3f772259a0fa963aff9ce0a`. The subsequent test-only `84a34a3`
and this qualification documentation preserve those product subtrees. Earlier
`3d62f6a` qualification is superseded for the changed product by the final2 runs.
Here `E` is the external evidence directory given above.

All four fresh-source presets passed configure and build with exit zero:
`hardware-x86-release`, `runtime-x86-release`, `hardware-x86-debug` and
`runtime-x86-debug`. `E/final2-fresh-build-results.json` binds each result to the
archive hash. `E/final2-fresh-optional-result.json` separately records successful
configure/build with both `CSE_BUILD_A3D=ON` and `CSE_BUILD_BROADCASTING=ON`.
Plain presets reset these options OFF and are never counted as optional evidence.
The checked toolchain remains VM1, MSVC v145 and SDK 10.0.26100.0; no warnings,
assertions, FP policy or provider requirements were relaxed.

## Dependency and live-runtime closure

`E/final2-repository-audit.json` scans 3,642 source/build inputs and 90 retained
native artifacts at the reviewed source. Pinned SDL3 validation passes for all
94 selected files. Current dependencies use native SDL3 3.4.20; SDL2 and
SDL2-compat are absent from the audited active source/build/runtime closure.
The historical manifest ZIP remains unchanged at SHA-256
`fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2`.

`E/final2-fixture-audit.json` records **43 x86 PE binaries**, 14 approved stock
providers, nine explicit VS 14.51 x86 CRT providers, 19 source-built replacement
paths and 17 required runtime hashes (16 default outputs plus pinned SDL3).
All ordinary/delay imports and export-forwarder edges resolve under the unchanged
closure policy. Miles, Steam, Chrome/libcef and codecs remain included. The 13
reviewed exclusions include SDL2.dll, stock software-client `sw.dll` and 11
non-Windows artifacts. Those exclusions describe this OpenGL fixture's scope;
they do not establish software-client or native non-Windows behavior.

All four final CS/HL Release/Debug gameplay sandboxes passed their own post-staging
runtime audits, saved in `E/final2-runtime/*-sandbox-audit.json`.
`E/final2-runtime-verification.json` verifies all 16 staged source-built outputs and
all 12 observed source-built modules against the appropriate runtime manifest in
each run. Each live snapshot has 154 modules and only the pinned SDL3 provider,
SHA-256 `6d7e653aa9c762ceda4fffc054a504dcce273e016eb9f1fe079b6409a058a9f7`.
These are actual loaded sets at the collection point; static closure and one live
snapshot do not prove every later dynamic load or unexercised provider feature.

## Gameplay, input boundary tests and review

Both games passed fresh final **Release and Debug** gameplay probes with full
sign-on, 651 alive network samples, 301 movement-command samples, nonzero simulated
origin changes and complete replay EOF before exit zero. CS on `de_dust2` reported
666 replay host frames; HL on `crossfire` reported 654. Network samples and replay
host frames are different clocks. Raw logs, demos, manifests, native module snapshots,
renderer captures and separate `*-demo-validation.json` results are retained in
`E/final2-runtime/`; the new gameplay recordings do not replace the immutable paired
benchmark recordings. The root visually inspected final internal 640×480 renderer
TGAs: CS world/HUD behind its MOTD and HL world/HUD. Accelerated Parallels CLI
screenshots remain unsuitable for judging rendered pixels.

Whole-branch review found one P2: VGUI2 could leave SDL's independent cursor
visibility hidden when VGUI1 regained input, and unchanged VGUI2 requests could
reclaim relative mode. Fix `cf7e58c` shares the visibility/raw-mode policy while
preserving each input owner's local request and grab behavior. The scoped re-review
through `84a34a3` approved the fix with zero open findings and no new material
regression. The native interactive SDL3 boundary fixture records compile **0**,
before-policy **1** with six failures, and fixed-policy **0**, **15/15 PASS** in
`E/cursor2-result.json` and its build/before/fixed logs. This exercises actual SDL
state through production-consumed policy helpers. The specific VGUI2-to-VGUI1
ownership handoff was not demonstrated in a live game; the later observed GameUI
menu and raw/legacy console interactions are separate runtime evidence.

Strict Windows native helper tests also qualify layout/physical keyboard mapping,
reverse scancodes, UTF/text/event arithmetic, fractional mouse/wheel behavior,
gamepad mapping/reset and retained SDL audio ownership/ring boundaries. Windows
game audio remains native DirectSound/WaveOut/Miles. Dummy SDL stream tests qualify
the retained non-Windows ownership contract on Windows; they do not establish
native Linux/macOS runtime or actual Windows game sound output.

## Final acceptance matrix and coverage limits

Final build, gameplay, closure, review and paired measurements are established.
The migration is **qualified for the exercised Windows x86 VM paths with the stated
coverage limits**. Real focus-loss/return and decoded startup-movie observations
also passed; these are separate from helper tests and process markers.
A failed desktop-input delivery harness remains unverified unless independent app
evidence demonstrates a regression.

| Behavior | Observed evidence | Qualification / limit |
| --- | --- | --- |
| Desktop fullscreen | Baseline and final SDL3 1357×827 internal captures show clear textured world/HUD | **Pass** for the observed desktop-fullscreen renderer path in `E/final2-display/` |
| Exclusive/no-FBO closest-mode fallback | Baseline and candidate reach full EOF/exit zero at actual closest mode 900x1440 | Process/mode/replay pass; both TGAs show diagonal/stale-pixel corruption and live CUA views are black, so visual acceptance is **unverified** |
| Unsupported window size | SDL2 and final SDL3 requested 641×479 windowed and selected the same 900×1440 closest mode | **Process/mode/replay pass**: full EOF, 654 host frames and exit zero for both; matching geometry does not independently qualify pixels |
| Native Windows sound output | Matched SDL2 no-collector and final SDL3 with/without-collector WAVs are nonzero, with complete replay/clean exit and continuous owned foreground | **Pass** for native DirectSound output in `E/final2-audio-summary.json`; original silent attempt remains unsuccessful, cause unestablished |
| Dedicated startup / shutdown | CS `de_dust2` and HL `crossfire`, two-player localhost servers | **Pass**: ready/map/status logs, clean exit zero and no forced cleanup in `E/final2-dedicated2/` |
| Startup movie decode / render | Both versions complete the same 10.024 s `valve.webm`; root CUA also saw a decoded Valve figure/red-valve/logo frame inside the final SDL3 640×480 window | **Pass**: actual candidate movie frame observed, playback/completion logs and exit zero/no force in `E/startup-media-visual-final2/` |
| Console ASCII / raw input / menu opening | Official Parallels virtual-key press/release works: baseline typed marker and quit; candidate typed `rawok`, queried raw=1, toggled raw input and visibly opened menu | Observed phases valid; candidate `keycodes2` hit its 90 s bound during later legacy/quit and required cleanup, so that run does **not** qualify clean exit |
| Legacy console input / typed quit | Final candidate receives raw=1, raw=0, typed `legacyok`, then typed quit | **Pass** in `E/hid-runtime/candidate-keycodes3-result.json`: exit zero, no forced cleanup and normal subsystem shutdown logs |
| Clipboard / non-ASCII game-UI entry / double-click / detailed pointer transitions | Native keyboard/text/cursor boundary tests passed | Unexercised desktop cases remain **unverified** |
| Focus loss / return in raw and legacy modes | Instrumented final game has three owned-foreground segments separated by two Alt-Tab away/back cycles; console markers continue in both modes | **Pass** for exercised loss/return: `E/final2-focus-summary.json`, typed quit, exit zero and no force |
| Minimize / physical mouse capture and deltas | Native policy/mapping/cursor boundary tests | Actual minimize and physical capture/delta cases remain **unverified** |
| Gamepad / hotplug / high DPI / hardware refresh and gamma | Mapping/reset/event-coordinate tests | Unavailable device/native-hardware cases remain unverified |
| Seven paired fixed-demo runs per game | Both games completed 16 runs each: two excluded warmups plus 14 measured runs | **Pass**: seven complete AB/BA pairs/game, full foreground/EOF/exit gates; final descriptive VM results below |

The first dedicated-driver attempt read a null `ExitCode` despite complete status
logs because its PowerShell process handle was not cached. It is superseded by
`final2-dedicated2`, which caches the launched handle and observes exit zero for
both games. No server code or termination gate was changed to obtain that result.

Native audio output now has three successful controlled observations in
`E/final2-audio-summary.json`: SDL2 without live module collection (23.55 s,
RMS 0.101981), final SDL3 without collection (23.62 s, RMS 0.115859), and final
SDL3 with collection (22.49 s, RMS 0.110848). All are nonzero stereo 48 kHz
float PCM, with complete gameplay/replay, clean exit and one uninterrupted owned
game foreground segment. These qualify observed native DirectSound output;
loudness is not a deterministic differential comparison. Separate final gameplay
runs establish actual loaded-module closure.

The first `E/final-audio/valve-final-audio.wav` remains an unsuccessful all-zero
23.04 s capture with no contemporaneous foreground log. Its cause is **not
established**. Both later collector-enabled and collector-disabled captures pass;
the evidence does not support blaming the collector or focus for the earlier
silence. The audio prerequisite was satisfied before the paired benchmarks.
Both complete series ran after console-input qualification, with no concurrent
builds, tests or other games.

The exclusive/no-FBO previews `E/baseline-display/exclusive-preview.png` and
`E/final2-display/exclusive-preview.png` both show corrupt captured pixels. Matching
dimensions and successful replay do not qualify exclusive rendering. Paired live
CUA observations are also black while player-ready, recording and replay logs
continue progressing. This VM observation cannot distinguish capture
from actual display output; no exclusive visual pass or SDL3-specific regression
is inferred from the shared symptom.

The official [Parallels keyboard key-code table](https://docs.parallels.com/landing/parallels-desktop-developers-guide/command-line-interface-utility/manage-virtual-machines-from-cli/general-virtual-machine-management/send-a-keyboard-event-to-a-virtual-machine/list-of-parallels-keyboard-key-codes) for `--key`, with explicit press/release,
successfully delivers real guest input, superseding earlier unsuccessful CUA,
AT-scancode and native SendInput delivery experiments. Baseline `keycodes2` records
`hidok` and a typed clean quit; candidate `keycodes2` records `rawok` and
`m_rawinput=1`, and the root saw the opened menu. Its later timeout/forced exit
remains unsuccessful for shutdown; only the observed phases count. The shorter
`keycodes3` run then passed: raw input reports 1 then 0, typed `legacyok` is echoed,
and typed quit reaches normal World/Server/DemoPlayer shutdown, exit zero and no
forced cleanup. Its official virtual-key events use explicit press/release with
30 ms hold/gap. These real ASCII/raw/legacy/menu observations qualify their exercised
paths. A separate instrumented focus run then passed two Alt-Tab away/back cycles,
first legacy then raw, with three owned-game foreground segments, `legacybegin`,
`legacyfocusok`, `rawfocusok`, raw=1 then 0, and typed clean quit. It qualifies
observed focus loss/return, not minimize or physical mouse deltas. Clipboard,
non-ASCII game-UI entry, double-click, unexercised capture/device cases and remaining
UI interactions stay unverified. Native Linux/macOS execution, inactive
software-client paths and native hardware remain unqualified. `docs/sdl3-final-evidence.json` preserves
machine-readable provenance, established results and remaining limits.

The root's final startup-movie visual observation saw an actual decoded Valve
figure with red valve/logo inside the SDL3 Half-Life 640×480 window. The visual
repeat (`E/startup-media-visual-final2/candidate-result.json`) reports PID 5704,
13.219 s process duration, clean exit zero and no forced cleanup, with the same
WebM SHA-256 `f778bed59c274d8477416fff74bb631a22283a370aa9fe03d815142194078df5`.
The log reports media length 10.024 s, completion and no playback errors. The CUA
image is retained in the chat tool history; no local screenshot file is claimed.

## Migration decisions and retained costs

These five rulings preserve the working ledger's scope and implementation decisions:

| Ruling | Basis / constraint | Cost if the interpretation is wrong |
| --- | --- | --- |
| The owner's explicit go-ahead authorized the assessed direct SDL3 execution; no repeated plan approval was needed. | Existing session authorization was sufficient. | Reversible branch rework for a wrong scope interpretation. |
| Complete SDL2 removal covers retained active and inactive dependency-bearing source and binaries; the original historical manifest archive stays immutable. | Historical text is not an active dependency. | Archival-policy changes if this boundary is reinterpreted. |
| Task 2 supplies dependency/build support; Task 3 atomically activates window/event/client/UI/Windows-media consumers. | Shared SDL types made the original core-first task boundary uncompilable; behavior and final scope did not change. | Reversible intermediate-commit rework for a wrong boundary. |
| The engine wheel retains one up/down pulse per nonzero whole event even when magnitude exceeds one; VGUI retains the full count. Fractional SDL3 accumulation emits no spurious down pulse for zero/horizontal-only events. | Preserve legacy integer batching while correcting the exposed zero-event bug. | Reversible input-policy rework. |
| Broad UI/display/media/hardware qualification runs against the final SDL3-only package rather than being inferred from the intermediate API port. | Staged helper/build/gameplay/layout tests supported implementation review; final behavior scope and acceptance stayed unchanged. | Reversible port fixes if final cases fail; behavior requirements are not discarded. |

Delivery stays on the isolated `codex/sdl3-migration` branch with the original
checkout preserved. Publishing and merging are outside the approved request.

## Qualification environment cleanup

VM1 cleanup removed 27 task-owned Scheduled Tasks; none remain, and no `hl`/`hlds`
process remains. The approved toolchain, fresh build sandboxes and VM evidence are
retained for reproduction (`E/vm1-task-cleanup-result.json`). Cleanup removed only
the two task-specific custom shares, `cse-sdl3-source` and `cse-sdl3-evidence`,
disabled host-folder sharing, and successfully issued `--isolate-vm on`
(`E/vm1-sharing-cleanup-result.json`, `E/vm1-after-cleanup.txt`). The root also
verified actual post-cleanup `config.pvs` has `IsolatedVm=1`, recorded in
`E/vm1-isolation-final-verification.json`. Temporary task execution and host access
have been removed.

## Performance method and precision

Use `tools/qualification/invoke_paired_benchmark.ps1` and
`tools/qualification/analyze_benchmark.py` as documented in the qualification README.
Stage matching data/settings/external providers, use the immutable fixed demo bytes,
exclude one warmup per version/game, and alternate seven measured AB/BA pairs.
Reserve a new evidence prefix for retries and run no concurrent builds/tests/games.
Hash/source/provider inventories are collected outside measured process lifetime;
provider live snapshots and renderer/audio capture occur in separate qualification
runs with matching sandbox runtime hashes.

The driver retains raw integer `host_speeds` profiles, aggregate CLI timings, process
wall duration, ordered EOF markers, PID/visible/foreground observations and unique
run manifests. The analyzer rejects incomplete series, partial replay, forged summary
counts, mismatched fixed-demo identities, missing warmups and background-window
runs. It reports median **paired** percentage changes; startup-inclusive process
wall duration is separate from timedemo time.

Ordinary integer FPS `f>0` implies frame duration `(1000/(f+1),1000/f]` ms. Zero FPS
means over 1000 ms and is retained with an unbounded upper duration. FPS 999 can be
a fast-frame sentinel, so its conservative interval is `[0,1000/999]` ms. Reported
p50/p95/p99 use nearest-rank lower/upper bounds, with optional midpoint **estimates**;
JSON null upper limits mean unbounded. Profiles include loading/sign-on host frames
before EOF. Rounded CLI aggregate timing is a separate measure.

Logging/profiling and 100 ms window observations add overhead. PresentMon 2.6.0 on
this VM emitted only seven sparse presentation events for 667 engine frames; these
are excluded from engine frame-pacing claims. Earlier nine-pair SDL/OpenGL API
microbenchmarks remain separate evidence. Neither the instrumented fixed-demo VM
results nor API microbenchmarks establish native-hardware performance or guarantee
statistical significance.

## Final paired VM benchmark results

`E/benchmark-final2-analysis.json` revalidates both complete series;
`E/benchmark-final2-summary.json` and `E/benchmark-final2-pairs.csv` preserve the
summary and every paired change. Each game has one excluded warmup/version, then
seven measured pairs with alternating order: **32 clean runs total** across both
games. Every CS run reports 666 CLI host frames / 667 profiles; HL reports 654 /
655. All runs reach full EOF, exit zero and pass owned visible/foreground-window
checks. Shared assets, external providers, native manifests before/after and
settings meet the driver parity/stability gates. The settings SHA-256 for both
series is `decf487b69def3848205d7a39c8848db33809688b19e6c90432f124616e5ecd8`.
Candidate metadata references fresh source `c4f729d`, including the cursor fix;
the baseline is `23a2135`.

| Game | SDL2 median CLI FPS | SDL3 median CLI FPS | Median paired FPS change | Median paired mean-frame change | Paired FPS change range |
| --- | ---: | ---: | ---: | ---: | ---: |
| CS | 177.274 | 177.026 | −0.234% | +0.234% | −17.575% to +14.725% |
| HL | 272.810 | 266.452 | −3.674% | +3.814% | −11.223% to +18.817% |

The FPS medians summarize each version independently; paired changes are computed
per corresponding pair before taking their median. They need not equal the ratio
of the two independent medians. Pair-to-pair variation is wide. These are
**descriptive, instrumented VM observations**, with no statistical-significance
claim, controlled uninstrumented comparison or native-hardware prediction.

The following millisecond intervals are the **median of seven per-run nearest-rank
quantile bounds**, not pooled percentiles. Bounds are rounded outward to 0.001 ms;
the JSON retains full values. Each profile distribution includes loading/sign-on
host frames before EOF.

| Game / version | p50 frame-time bounds, ms | p95 bounds, ms | p99 bounds, ms |
| --- | ---: | ---: | ---: |
| CS SDL2 | 3.558–3.572 | 4.385–4.406 | 21.739–22.223 |
| CS SDL3 | 3.546–3.559 | 4.464–4.485 | 21.739–22.223 |
| HL SDL2 | 3.236–3.247 | 3.773–3.788 | 8.333–8.404 |
| HL SDL3 | 3.184–3.195 | 3.802–3.817 | 9.259–9.346 |

Measured CS profiles retain **five SDL2 and six SDL3 zero-FPS frames** among 4,669
profiles/version. Their maximum-duration upper bound is unbounded; they are not
removed to improve tail results. Measured HL profiles have no zero-FPS frames
among 4,585 profiles/version. No measured run contains the 999 sentinel. Integer
FPS quantization, loading-frame inclusion and logging/window-observation overhead
remain as described above. Sparse PresentMon events and earlier API microbenchmarks
are excluded from this game comparison.
