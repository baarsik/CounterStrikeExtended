# Runtime qualification utilities

Run the Windows tools on VM1. `vm1.py` sends a local PowerShell file through
Parallels' UTF-16 encoded-command transport; it does not configure shares, change
isolation, or install dependencies. `--vm` defaults to `Windows 11`;
`--system` explicitly selects system transport instead of the current user.
Use `--interactive` for a short wrapper that invokes the probe on the logged-on
Windows desktop through Task Scheduler. The transport default timeout is 180
seconds; keep it above staging, the probe timeout and cleanup time. Oversized
scripts are staged in uniquely named guest temp files using 384-byte chunks;
every Parallels encoded argument stays at or below 2,800 characters. Interactive
tasks execute the staged file. Only these owned temp files are removed afterward.

An example wrapper, with paths supplied by the operator:

```powershell
& 'C:\source\tools\qualification\invoke_game_probe.ps1' `
    -Game cstrike `
    -RuntimeDirectory 'C:\source\output' `
    -GameDataDirectory 'C:\game-data\Half-Life' `
    -SandboxDirectory 'C:\qualification\cs-sandbox' `
    -EvidenceDirectory 'C:\qualification\evidence' `
    -ModuleCollectorPath 'C:\qualification\runtime_modules.exe' `
    -SourceCommit 'COMMIT' -TestCaseLabel 'release-1'
```

Run it with `python3 tools/qualification/vm1.py wrapper.ps1 --interactive`.
For Half-Life, use `-Game valve`; Steam app IDs default to 10/70 respectively.
The sandbox must be empty on first use; subsequent runs require its tool-created
ownership marker. The tool copies data, then overlays the explicitly supplied
runtime. It requires all 16 default full-runtime outputs and verifies their copied
sandbox hashes against the supplied runtime directory. Hardware-only output cannot
silently borrow missing stock modules. It does not substitute or remove providers.
Use a new testcase label or evidence directory for each run. A prefix reservation
is created atomically and retained even for interrupted runs; existing start,
module, log, demo or result artifacts also prevent reuse. Source identity is supplied metadata, not an
automatic source/binary attestation.

The probe forces 640x480, multiplayer/loopback, records 650 scripted host frames,
then waits at least 1,200 frames for timedemo. Acceptance requires actual
`Host_EndGame: End of demo.`, a timedemo count consistent with the demo's NORMAL
network-frame count (allowing the documented first-host-frame exclusion), the
final probe marker, and exit zero. On failure it closes, then if needed terminates,
only the process object it launched. Process logs, the demo, screenshots and result
JSON remain in the evidence directory. Screenshots still need visual inspection;
the result does not automatically qualify rendering or collect frame distributions.

Before launch, the probe clears prior `HalfLife*.webp` captures only in the owned
sandbox's game directory. It copies new WebP captures to the evidence directory
with the run prefix, preserving their original bytes.

Compile the optional collector directly from an x86 MSVC developer environment:

```powershell
cl /nologo /W4 /WX /EHsc /Fe:runtime_modules.exe tools/qualification/native/runtime_modules.cpp
```

`runtime_modules.exe PID [UTF8-output-file]` reads the target's actual live module
snapshot and returns nonzero for invalid/unavailable PIDs. The probe invokes it
after the player-ready marker while recording is still active, then captures each
module's file version and SHA-256. This records the observed loaded set; later
dependency auditing must interpret it and account for modules loaded afterward.

Validate the copied recording separately on the host:

```sh
python3 tools/qualification/validate_demo.py EVIDENCE/cstrike-release-1.dem \
  --min-network-frames 100 --min-duration 5 --require-alive --require-movement
python3 -m unittest discover -s tools/qualification/tests -v
```

On Windows, run `powershell.exe -NoProfile -File tools/qualification/tests/test_evidence.ps1`
to check interrupted-run and completed-run prefix preservation without launching a game.

The demo validator checks recorded structure/state. It does not replace runtime
playback, visual inspection, input/focus/device acceptance or provider auditing.

## Source and binary inventory

Capture a committed source reference outside the checkout, then verify it:

```sh
python3 tools/qualification/capture_reference.py /absolute/evidence/reference --revision COMMIT
python3 tools/qualification/capture_reference.py /absolute/evidence/reference --verify
```

This archives sources and hashes; it neither builds nor qualifies a runtime.
Generate the current CMake inventory for the Windows runtime's architecture and
configuration, then inspect the explicitly supplied runtime:

```sh
python3 tools/cmake_inventory.py --target-os windows --architecture x64 \
  --configuration Release --output build/runtime-inventory.json
python3 tools/qualification/audit_runtime_inventory.py /absolute/game-runtime \
  --inventory build/runtime-inventory.json --architecture x64 \
  --output /absolute/evidence/runtime-inventory.json
```

The binary audit requires `--inventory`; it has no fallback source list. Match
the CMake inventory's OS, CPU and configuration to the runtime being inspected.
The [CMake inventory tool](../../docs/build-system.md#inspecting-effective-target-settings)
uses host-compiler metadata and symbolic dependencies; it does not cross-compile
or verify provider binaries. The Windows binary audit reports PE imports,
exports, resources, architecture and hashes as static evidence, separate from
actual OS/provider loading and gameplay.

Compare a rebuilt ARM64 game module's exports with a retained Win32 dump:

```sh
python3 tools/macho_game_exports.py --reference /absolute/evidence/Release-hl-win32.txt \
  --library build/game-modules-macos-arm64-release/runtime/Release/valve/dlls/hl.dylib
```

The reference is actual `dumpbin /nologo /exports` output. This checks machine,
minimum macOS version and C/canonical C++ exports without loading the game.

## Native SDL3 closure and isolated provider/data fixture

Run the static repository audit on the authoring host (requires `llvm-readobj`):

```sh
python3 tools/qualification/audit_sdl3.py --output repository-audit.json repository \
  --llvm-readobj /path/to/llvm-readobj
```

This scans retained source/CMake definitions and all tracked native artifacts, including
optional targets, COFF import identities, ELF/Mach-O needed libraries, and active
Windows library architecture. Retained x64 package tooling is inventoried separately
from selected x86 runtime providers. Upstream SDL3 headers remain unmodified, including
`SDL_oldnames.h`; production must never opt into compatibility names. Original
`docs/msbuild/upstream-manifests.zip` remains immutable historical evidence. SDL2
platform providers and legacy prebuilt VGUI/control libraries have been removed.
Runtime builds use source-built VGUI providers.

A Steam data tree can contain `SDL2.dll`, stock `sw.dll` and other alternative
modules even after overlaying a successful SDL3 build. For SDL3 qualification,
prepare a **new isolated fixture** with explicit decisions for every native data
artifact. Never edit the Steam source, baseline runtime or fixed assets:

```powershell
python tools/qualification/audit_sdl3.py --output overlay-inventory.json inventory C:\game-data\Half-Life
```

Review the inventory and write a decisions JSON object with `providers` (relative
paths of licensed required providers to retain, including Steam/codecs) and
`excluded` (relative paths mapped to a concrete reason). The source-built output
at an identical path automatically replaces a stock artifact. Every other native
artifact needs an explicit choice; no wildcard deletion or automatic provider
purge is performed. An example of the schema, **not a complete provider list**:

```json
{
  "providers": ["steam_api.dll"],
  "excluded": {
    "SDL2.dll": "Replaced by pinned native SDL3; no current consumer imports SDL2",
    "sw.dll": "Stock software engine alternative outside this OpenGL qualification fixture"
  }
}
```

Excluding a stock alternative is a declared fixture limitation, not evidence that
its feature was migrated. Retain all required Steam/voice/video providers;
a missing or SDL2-dependent required feature must be rebuilt/replaced within its
authorized boundary, or reported as a failed/incomplete case. Dynamic provider
requirements need the runtime behavioral matrix and live-module evidence as well.
The fixture copies **all non-native game data** and the explicitly retained native
providers, then overlays the complete source-built runtime:

```powershell
python tools/qualification/audit_sdl3.py --output fixture-audit.json fixture `
  --data C:\game-data\Half-Life --runtime C:\source\output `
  --destination C:\qualification\sdl3-fixture --decisions overlay-decisions.json
```

The destination must be empty and separate from sources. The report preserves
hashes/reasons for exclusions, retained provider hashes, replacement paths, and
all 16 source-built runtime output hashes plus SDL3.dll. It rejects SDL2/compatibility
artifacts anywhere, non-x86 PEs, and unresolved non-system imports/delay imports,
including dependencies of unused retained providers. System-import names are an
explicit list/API-set families, not an arbitrary missing-DLL exemption. This is
formal closure evidence; it does not execute providers or prove dynamically loaded
module availability/behavior. The tool leaves a failed fixture for diagnosis;
correct decisions and use a new empty destination on retry.

Use this fixture as `-GameDataDirectory` for `invoke_game_probe.ps1`, keep the
original source-built directory as `-RuntimeDirectory`, and use a **new empty
sandbox**. The generic probe intentionally does not purge old modules from an
existing sandbox. Audit the actual sandbox after probe staging as well:

```powershell
python tools/qualification/audit_sdl3.py --output sandbox-audit.json runtime C:\qualification\new-sandbox
```

The `runtime` audit works with Python's standard library on Windows; no LLVM or
provider execution is required. Runtime module snapshots and provider hashes must
still confirm the actual loaded set. Windows effects use DirectSound/WaveOut;
music uses libmpg123/SDL3. `tests/sdl3/audio_stream_tests.cpp` checks retained
platform-stream ownership/pause/cursor behavior with SDL's dummy device.
Use the [music checks](#music-migration-closure) for the current music backend.

## Paired fixed-demo VM benchmark

This tool runs fixed-demo comparisons between separately staged SDL2/SDL3 builds.
It requires matching non-SDL providers, so it cannot compare the current mpg123
runtime against a Miles baseline without changing the provider comparison.

`invoke_paired_benchmark.ps1` consumes **already staged, qualified, owned** baseline
and candidate sandboxes. Use fresh directories: copy the reviewed SDL3-only fixture
and overlay each complete source-built runtime separately. The baseline needs its
pinned SDL2 provider; the candidate must contain only native SDL3. Stage identical
approved external providers and CRTs in both. The driver rejects differences in
native path/hash sets except the 19 known source-built outputs and SDL2/SDL3 DLLs.
It also compares fixed-map and PAK/WAD hashes. This does not replace the complete
fixture audit, matching data staging provenance or separate live-module collection.
Both sandboxes require the `.cse-qualification-sandbox` marker from the game probe.

Supply one reviewed ASCII settings file for both versions, explicitly setting the
intended render quality and audio choices. Archive it with the staging evidence.
The driver overwrites `config.cfg` and `autoexec.cfg` in these owned sandboxes before
**each** run and executes a byte-identical settings copy after startup. It enforces
640x480 windowed/forceres, multiplayer two-player mode, vsync zero, fps_max 100 and
fps_override zero; timedemo controls replay timing. A copied settings artifact and
its SHA-256 accompany the series. Source commit parameters are supplied provenance;
compare sandbox runtime hashes to the corresponding fresh-build manifests yourself.

```powershell
& 'C:\source\tools\qualification\invoke_paired_benchmark.ps1' `
    -Game cstrike `
    -BaselineSandboxDirectory 'C:\qualification\benchmark-sdl2-cs' `
    -CandidateSandboxDirectory 'C:\qualification\benchmark-sdl3-cs' `
    -DemoPath 'C:\qualification\csebaseline.dem' `
    -DemoSha256 'fb1a18edf31182d49b022a46850df0cd263fe1ea101b28caac78af038fdf211c' `
    -SettingsPath 'C:\qualification\fixed-settings.cfg' `
    -EvidenceDirectory 'C:\qualification\benchmark-evidence' `
    -TestCaseLabel 'release-final' -BaselineCommit 'BASELINE' -CandidateCommit 'CANDIDATE'
```

For Half-Life use `-Game valve`, `csehlbaseline.dem` and SHA-256
`3177530c2d13a5e88f2ffc4f86f0baefe939034cde0b5c8d787583f4c035ffa3`.
The driver pins each game to these accepted recordings; it cannot silently substitute
new random recordings. Default is one excluded warmup/version, then seven pairs,
alternating candidate/baseline and baseline/candidate. `-Pairs 1` is a harness smoke,
not final performance qualification. Serialize VM work; close other hl/hlds processes
and do not build or run other tests during the series. Keep the VM desktop active.

The process wait is bounded and observes its visible window/foreground PID every
100 ms, recording observations; foreground loss/minimization after acquisition
fails. No logs, module hashes or presentation tracing are read during measurement.
Complete EOF, ordered markers, one CLI report of at least 650 host frames, plausible
profile coverage, exit zero and no forced cleanup are required. Profile count may
be CLI frames minus one through plus two; the 651 network samples are a different
clock from replay host frames. Each prefix is reserved; interruptions leave unique
raw logs/results. Only the launched PID is cleaned up. `wall_seconds` includes
startup/waits/shutdown; use the separate CLI timings for replay comparison.

```sh
python3 tools/qualification/analyze_benchmark.py \
  EVIDENCE/cstrike-release-final-series.json EVIDENCE/valve-release-final-series.json \
  --output EVIDENCE/paired-analysis.json
```

The analyzer rechecks raw logs, ordered AB/BA pairs, warmup exclusion, exact demo
identity, exit/foreground evidence and profile coverage. It reports per-pair FPS and
mean-frame-time changes and their median, plus each run's p50/p95/p99 profile bounds.
`host_speeds` prints integer FPS, not a high-resolution frame trace. Ordinary FPS
`f > 0` bounds frame time to `(1000/(f+1), 1000/f]` ms; FPS zero means over 1000 ms
with an unbounded upper limit. FPS 999 can also represent a sub-0.1 ms sentinel, so
its conservative interval is `[0, 1000/999]` ms. Bounds use nearest-rank quantiles;
midpoints are labeled estimates, and JSON `null` upper bounds mean unbounded.
Zero/slow frames stay in the distribution. Profiles include load/sign-on frames
between the start marker and EOF. Rounded aggregate CLI timings remain separate.

These are instrumented VM1 Windows x86-emulation measurements. Host profiling,
console logging and window observations cost time. Seven descriptive pairs provide
no native-hardware prediction or statistical significance guarantee. PresentMon's
sparse OpenGL/ComposedCopy GPU-GDI events are excluded from engine pacing claims;
the earlier SDL/OpenGL microbenchmark is separate API evidence. Renderer screenshots
and live provider inventories belong in separate gameplay qualification runs to
avoid screenshot/hash IO during the benchmark.

## Music migration closure

Prepare the isolated fixture above with explicit exclusions for `Mss32.dll`,
`mssmp3.asi`, `mssvoice.asi` and `binkawin.asi`. Stage the complete runtime,
source-built `mpg123.dll` and required CRT providers, then run:

```powershell
python tools/qualification/audit_mpg123.py C:\qualification\music-fixture --output music-closure.json
```

This checks SDL3/formal import closure, rejects Miles modules/imports and requires
both engines to import the staged mpg123 provider. See [build instructions](../../docs/building.md#component-builds-and-tests)
for decoder/player/worker tests.

`invoke_music_probe.ps1` runs a one-second upstream sweep loop, pause/resume,
quarter volume, fade, an existing Half-Life music file, file replacement and stop.
After auditing, mark the owned sandbox with `.cse-music-fixture` and copy upstream
`src/tests/sweep.mp3` to `<game>/media/cse-music-test.mp3`. Supply `-Game`,
`-SandboxDirectory`, `-EvidenceDirectory`, `-NAudioAssembly` (test-only NAudio 1.10)
and `-ModuleCollectorPath`. The probe mutes effects, timestamps capture buffers and
records the loaded decoder identity. Analyze the resulting `<game>-<label>` prefix:

```powershell
python tools/qualification/analyze_music_probe.py C:\evidence\valve-music --output C:\evidence\valve-music-analysis.json
```

The analysis checks amplitudes, silence phases and replacement identity. Use the
separate gameplay probe for movement, recording/replay and clean exit.
