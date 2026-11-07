# libmpg123 music migration validation

Validated on VM1 on 2026-11-07: Windows 11 ARM64 executing the project's Windows
x86 binaries under emulation, MSVC v145 and Windows SDK 10.0.26100.0.
The tested engine implementation is `af5210a`; subsequent changes affect tooling
and documentation. The nine changed engine source/header hashes match VM1's build
inputs (`qualified-engine-source-hashes.json`).

## Implementation

Both engines use shared mpg123 1.33.7 with SDL3 streams; Miles calls/imports and
bundled artifacts are removed. Existing music controls remain, with source-rate
signed-16 PCM and SDL device conversion. Windows effects use DirectSound/WaveOut;
physical CD controls retain MCI. See [building](building.md#source-built-mp3-decoder)
for dependency staging and [provenance](../GoldSrc-SDK/external/mpg123/UPSTREAM.md)
for the upstream source/license and Xash3D reference boundary.

## Build and automated checks

| Check | Result |
| --- | --- |
| Four checked-in hardware/runtime Release/Debug CMake presets | Passed |
| Standalone hw_engine and swds MSBuild projects, Release/Debug | Passed |
| Real decoder/player/worker tests | 3/3 passed in each configuration |
| Python build/translation and qualification suites | 15 and 55 passed |
| PowerShell evidence checks, including concurrent console writer | Passed |
| Deleted mpg123 DLL restoration through CMake and MSBuild | Passed |
| Runtime import closure and live module identity | Both engines import the staged mpg123 DLL; Miles modules absent |

The one-second VBR fixture produces 176400 PCM bytes, including its final partial
buffer. Tests cover rewind, looping, priming, pause/resume, replacement, subsystem
ownership, queued filename lifetime/order and joined shutdown. A deliberately
premature completion mutation fails against an independent SDL drain reference;
the actual implementation passes.

## Captured music output

WASAPI loopback captured each game's output with effects muted and timestamps
accounting for idle intervals. The one-second sweep looped for about three
seconds, paused/resumed, changed volume and faded to silence. The probe then
played shipped `Half-Life01.mp3`, replaced it using `mp3 loopfile` with the sweep
at 60 percent volume, and stopped. Replacement title and amplitude were checked.

| Game | Configuration | Quarter/full RMS | Replacement/full RMS |
| --- | --- | ---: | ---: |
| Half-Life | Release | 0.2509 | 0.6228 |
| Counter-Strike | Release | 0.2508 | 0.6232 |
| Counter-Strike | Debug | 0.2511 | 0.6231 |
| Half-Life | Debug | 0.2511 | 0.6228 |

Pause, faded and stopped phases measured zero RMS. All selected runs exited zero
and loaded the matching staged decoder, with no Miles module. These are bounded
functional/amplitude checks rather than a perceptual-quality benchmark.

## Gameplay and dedicated servers

CS and Half-Life each passed Release and Debug recording/replay and clean exit.
Each recording contains 651 alive network samples and 301 movement-command
samples; replay reached full EOF. Module/import checks passed and world/HUD
captures were inspected (CS includes its MOTD overlay). Release dedicated servers
started the expected localhost maps, answered status and exited zero.

Isolated fixtures excluded Miles providers and stock optional/software alternatives
outside the default-runtime tests. Original Steam data was not edited; see
[qualification utilities](../tools/qualification/README.md#music-migration-closure).

## Findings and coverage limits

- Owned queued paths correct the stop/playfile filename race; queue regressions
  and captured replacements pass. `cd mp3info` now reports volume with the
  correct floating-point format.
- One CS Release launch ended before music markers with `ucrtbase.dll` exception
  `0xc0000417`. The unchanged candidate passed its music retry and subsequent
  CS Release/Debug gameplay launches. The startup failure's cause is unresolved;
  its result and Windows error details remain in the evidence.
- Optical CD hardware, endpoint hotplug/default-device changes, and native
  x64/ARM64/macOS execution were not runtime-qualified. Production presets remain
  Windows x86. Other provider/source rights remain scoped by [licensing](licensing.md).

## Evidence

Build logs, successful/failed attempts, WAVs/timestamps, demos, module snapshots,
source hashes and import reports remain outside the repository:

```text
/Users/mmalakhouski/.codex/artifacts/cse-mpg123-20261107
C:\CI\CSE-Mpg123-20261107
```

Key records: `final-build-routes-result.txt`, `msbuild-results.json`,
`final-native-tests-Release.log`, `final-native-tests-Debug.log`,
`tail-mutation-red.log`, `qualified-engine-source-hashes.json`,
`final-gameplay-summary.json`, `final-music-closure-*.json`,
`staging-verification.json`, and `music/*-analysis.json`.
Selected captures: `valve-music-owned-release`, `cstrike-music-owned-retry-release`,
`cstrike-music-owned-debug-v2` and `valve-music-owned-debug-v3`.
