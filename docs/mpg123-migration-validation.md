# libmpg123 music migration validation

Validated on VM1 on 2026-11-07: Windows 11 ARM64 executing the project's Windows
x86 binaries under emulation, MSVC v145 and Windows SDK 10.0.26100.0.
The tested engine implementation is `af5210a`; later changes are qualification
harness/prose changes. All nine modified engine source/header hashes match the
actual VM1 build source (`qualified-engine-source-hashes.json`).

## Implementation

Miles calls/imports and bundled `external/miles` artifacts are removed. Both
engines use source-built shared mpg123 1.33.7 and SDL3 music streams. The retained
music commands, track mapping, looping, priming, fades and focus hooks remain.
File/prime requests own their paths in the command queue, playback controls keep
order, and command/decoder producers are joined before resource release.

Decoding uses the file's sample rate and signed-16 PCM, with SDL handling device
conversion. This replaces the old fixed-22050-Hz Miles driver. Windows game sound
effects remain DirectSound/WaveOut; physical CD controls retain their MCI path.

The complete unmodified upstream source archive, COPYING, AUTHORS and provenance
are pinned/staged with `mpg123.dll`. Xash3D FWGS was an architectural reference;
its GPL integration, custom stub and Waf build were not copied.

## Build and automated checks

| Check | Result |
| --- | --- |
| Four checked-in hardware/runtime Release/Debug CMake presets | Passed |
| Standalone hw_engine and swds MSBuild projects, Release/Debug | Passed |
| Real decoder/player/worker tests | 3/3 passed in each configuration |
| Build/translation Python suite | 15 passed |
| Qualification Python suite | 55 passed |
| PowerShell evidence checks, including concurrent console writer | Passed |
| Deleted mpg123 DLL restoration through CMake and MSBuild | Passed |
| Runtime import closure and live module identity | Both engines import the staged mpg123 DLL; Miles modules absent |

The upstream one-second VBR sweep decodes to exactly 176400 PCM bytes (44100
stereo frames). Small/large reads agree through the final partial buffer and
rewinding reproduces the beginning. Player tests cover actual looping,
priming, pause/resume, replacement, subsystem ownership and repeated teardown.
A deliberately premature EOF-completion mutation failed against an independent
SDL drain reference (1129 ms reference versus 911 ms premature completion);
the actual implementation passed. A blocked worker regression exceeds the old
99 ms shutdown timeout and verifies teardown waits for command completion.
Queued stop/file/stop/file/stop requests preserve filename lifetime and order.

## Captured music output

WASAPI loopback captured the real game output with game effects muted. Buffer
timestamps account for idle intervals without packets. A one-second sweep ran
for multiple loops, paused/resumed, changed to quarter volume, faded to silence,
played shipped `Half-Life01.mp3`, switched through `mp3 loopfile` back to the
sweep at 60 percent volume, then stopped. Replacement title and amplitude were
both checked. All selected runs exited zero and loaded the matching source-built
decoder from the isolated fixture, with no Miles module loaded.

| Game | Configuration | Loop interval | Quarter/full RMS | Replacement/full RMS | Pause/faded/stop RMS |
| --- | --- | ---: | ---: | ---: | ---: |
| Half-Life | Release | 3.038 s | 0.2509 | 0.6228 | 0 |
| Counter-Strike | Release | 2.999 s | 0.2508 | 0.6232 | 0 |
| Counter-Strike | Debug | 3.004 s | 0.2511 | 0.6231 | 0 |
| Half-Life | Debug | 3.003 s | 0.2511 | 0.6228 | 0 |

These are bounded functional/amplitude checks, not a perceptual-quality benchmark.

## Gameplay and dedicated servers

CS and Half-Life each passed Release and Debug recording/replay and clean exit.
Each recording contains 651 alive network samples and 301 alive movement-command
samples, and replay reached full EOF. Native module snapshots and formal closure
checks passed in all four isolated sandboxes. Rendered CS/HL world/HUD captures
were inspected; the CS capture includes its MOTD overlay.

Release CS/HL dedicated servers started the expected localhost maps, answered
status and exited zero without forced cleanup. The fixture explicitly excludes
Mss32.dll, mssmp3.asi, mssvoice.asi and binkawin.asi, plus stock optional/software
alternatives outside these default-runtime tests. Original Steam data was not edited.

## Findings and coverage limits

- Independent review found a preexisting stop/playfile filename race, also
  reproduced by a silent CS replacement capture. Owned queued paths fix it;
  deterministic queue tests and both games' captured replacements pass.
- `cd mp3info` passed a float to `%i`; the corrected report prints the requested
  quarter volume. Harness log-sharing and exiting-window races were fixed and
  retested; those incomplete runs are excluded from the passing samples.
- One CS Release launch ended before music markers with `ucrtbase.dll` exception
  `0xc0000417`. The unchanged candidate passed its music retry and subsequent
  CS Release/Debug gameplay launches. The startup failure's cause is unresolved;
  its result and Windows error details remain in the evidence. Successful music
  cases do not establish that every launch is reliable.
- Optical CD hardware, endpoint hotplug/default-device changes, and native
  x64/ARM64/macOS execution were not runtime-qualified. Production presets remain
  Windows x86. Other provider/source licensing remains scoped by licensing.md.

## Evidence

Raw build logs, successful and failed attempts, WAVs/timestamps, demos, module
snapshots, source hashes and import reports remain outside the repository:

```text
/Users/mmalakhouski/.codex/artifacts/cse-mpg123-20261107
C:\CI\CSE-Mpg123-20261107
```

Key records: `final-build-routes-result.txt`, `msbuild-results.json`,
`final-native-tests-Release.log`, `final-native-tests-Debug.log`,
`tail-mutation-red.log`, `qualified-engine-source-hashes.json`,
`final-gameplay-summary.json`, `final-music-closure-*.json`,
`staging-verification.json`, and `music/*-analysis.json`.
The selected capture prefixes are:

- `valve-music-owned-release`
- `cstrike-music-owned-retry-release`
- `cstrike-music-owned-debug-v2`
- `valve-music-owned-debug-v3`

VM1's task-only share and transport scripts were removed, host sharing is OFF,
and VM isolation is restored to its original ON setting. Build/test sandboxes
and evidence remain available for reproduction.
