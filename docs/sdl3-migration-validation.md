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

## Native SDL3 source and package qualification

Product source for final qualification is commit
`3d62f6ae651ca6207a61613c0eff2b0d039c1586`, tree
`edff4be79e91d6b30792c2c63a77d8586848de35`. Its fresh `git archive` ZIP is
SHA-256 `8fc88a4950e5eb835b6c44c2e8eb33eecd20cc474e7fac0d9791b28c7ed13559`;
`E/final-product-source-manifest.json` records the full identity, SDK/CMake subtree
IDs and byte length. Here `E` is the external evidence directory above. Subsequent
qualification tool/document commits do not silently change that built product
identity. Fresh-build output identities must be retained with their own logs;
partial in-progress result files do not establish the remaining preset gates.

Task 4 closed native SDL3 source/dependency retirement and actual optional-ON
Release compilation at that product source. `E/task4-optional-fix3-result.json`
records configure/build zero; `E/task4-fixture-audit.json` records 43 x86 PE
binaries, 14 retained stock providers, nine explicit VS 14.51 x86 CRT providers,
16 required default source-built outputs plus pinned SDL3, and present/hashed
optional A3D/HLTV/proxy outputs. It rejects unresolved ordinary/delay imports and
forwarder dependencies. SDL2.dll and SDL2-compat are absent from that fixture.
The 13 reviewed exclusions include stock software-client `sw.dll` and non-Windows
artifacts; these exclusions are fixture scope, not feature-equivalence evidence.

All four default hardware/runtime Debug/Release builds previously passed at
`29d691a`; final qualification separately rebuilds fresh source at `3d62f6a`.
Do not count a plain runtime preset as an optional build: it resets
`CSE_BUILD_A3D` and `CSE_BUILD_BROADCASTING` OFF. Every actual optional qualification
configure repeats both `-D...=ON` arguments. Required MSVC v145/SDK 26100 settings,
warning/assertion/FP policy and provider requirements remain unchanged.

The pre-final native SDL3 live CS and HL runs reached full sign-on, recorded alive
movement, replayed to EOF and exited zero, with verified source-built loaded
modules and native SDL3-only snapshots. Strict Windows x86 helper tests cover
layout/physical keyboard mapping, reverse scancodes, text/event arithmetic,
mouse/wheel fractions, gamepad mapping/reset and retained SDL audio ownership/ring
boundaries. These tests establish the tested helper contracts; they do not assert
that unexercised desktop/hardware interactions passed.

## Final behavioral acceptance ledger

The root owns fresh builds, fixture staging, live probes, screenshots, audio
capture, final benchmarks and independent whole-branch review. Update the following
rows only from actual saved evidence; this implementation handoff makes no final
runtime or performance acceptance claim. A failed input-delivery harness is
**unverified**, unless independent app evidence demonstrates an app regression.

| Gate / behavior | Established evidence | Final qualification state at tool handoff |
| --- | --- | --- |
| Four clean-source presets, MSVC v145/SDK 26100 | Prior four default builds passed; exact product archive above | Root fresh-build run in progress; require four configure/build pairs |
| Explicit optional A3D + broadcasting ON | Task 4 actual Release ON build and fixture passed | Root fresh-source optional run required; do not infer from presets |
| CS/HL sign-on, alive movement, full EOF, clean repeated shutdown | Baseline and earlier native SDL3 live runs passed | Require fresh final Debug/Release runs and actual sandbox audits |
| Static repository and full provider closure | Task 4 repository/43-PE fixture passed | Recheck final source, actual staged sandbox and live loaded set |
| 640x480 OpenGL textured world/HUD | Internal renderer TGAs from baseline and earlier SDL3 | Require final renderer screenshots; accelerated CLI screenshots are insufficient |
| Desktop fullscreen / exclusive / closest-mode fallback | SDL2 HL desktop 1357x827 letterboxed; exclusive/no-FBO 900x1440 actual closest mode | Final SDL3 comparison pending |
| Native Windows sound output | SDL2 WASAPI loopback nonzero RMS/peak | Final SDL3 output capture pending; dummy SDL tests do not qualify native game sound |
| Dedicated startup / shutdown | Source and builds qualified | Final live startup/shutdown pending |
| Startup video decode | Source port/build; retained Chrome/codecs included | Actual video decode pending |
| UI text / clipboard / double-click / pointer transitions | Native keyboard mapping and text arithmetic tests passed | Real desktop UI input pending; prior CUA/native-input experiments failed delivery |
| Focus / minimize / restore / raw/legacy capture | Policy/mapping tests and source review | Real desktop event/capture matrix pending |
| Gamepad / hotplug / high DPI / hardware refresh and gamma | Mapping/reset/event-coordinate tests | Device/native-hardware cases unverified where unavailable |
| Seven paired fixed-demo runs per game | Accepted identical recordings and reusable driver/analyzer | Root smoke/full series and analysis pending |
| Whole-branch independent review | Prior task reviews closed | Final root review and resolution pending |

`docs/sdl3-final-evidence.json` is the compact provenance/limits handoff. Native
Linux/macOS runtime execution, inactive software-client paths and native hardware
remain unqualified. Source/helper coverage cannot substitute for those platform or
hardware runs. Windows audio remains native DirectSound/WaveOut/Miles; SDL3 dummy
stream tests exercise retained non-Windows ownership policy on Windows only.

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
