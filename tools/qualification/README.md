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
    -RuntimeDirectory 'C:\source\build\runtime-x86-release\runtime\Release' `
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

## Native SDL3 closure and isolated provider/data fixture

Run the static repository audit on the authoring host (requires `llvm-readobj`):

```sh
python3 tools/qualification/audit_sdl3.py --output repository-audit.json repository \
  --llvm-readobj /path/to/llvm-readobj
```

This scans retained source/manifests and all tracked native artifacts, including
optional targets, COFF import identities, ELF/Mach-O needed libraries, and active
Windows library architecture. Retained x64 package tooling is inventoried separately
from selected x86 runtime providers. Upstream SDL3 headers remain unmodified, including
`SDL_oldnames.h`; production must never opt into compatibility names. Original
`docs/msbuild/upstream-manifests.zip` remains immutable historical evidence. SDL2
platform providers and the two formally SDL2-linked VGUI artifacts are retired;
`linux/release/vgui.so` remains and has no formal SDL2 dependency.

A Steam data tree can contain `SDL2.dll`, stock `sw.dll` and other alternative
modules even after overlaying a successful SDL3 build. For SDL3 qualification,
prepare a **new isolated fixture** with explicit decisions for every native data
artifact. Never edit the Steam source, baseline runtime or fixed assets:

```powershell
python tools/qualification/audit_sdl3.py --output overlay-inventory.json inventory C:\game-data\Half-Life
```

Review the inventory and write a decisions JSON object with `providers` (relative
paths of licensed required providers to retain, including Miles/Steam/codecs) and
`excluded` (relative paths mapped to a concrete reason). The source-built output
at an identical path automatically replaces a stock artifact. Every other native
artifact needs an explicit choice; no wildcard deletion or automatic provider
purge is performed. An example of the schema, **not a complete provider list**:

```json
{
  "providers": ["mss32.dll", "steam_api.dll"],
  "excluded": {
    "SDL2.dll": "Replaced by pinned native SDL3; no current consumer imports SDL2",
    "sw.dll": "Stock software engine alternative outside this OpenGL qualification fixture"
  }
}
```

Excluding a stock alternative is a declared fixture limitation, not evidence that
its feature was migrated. Retain all required Miles/Steam/voice/video providers;
a missing or SDL2-dependent required feature must be rebuilt/replaced within its
authorized boundary, or reported as a failed/incomplete case. Dynamic provider
requirements need the runtime behavioral matrix and live-module evidence as well.
The fixture copies **all non-native game data** and the explicitly retained native
providers, then overlays the complete source-built runtime:

```powershell
python tools/qualification/audit_sdl3.py --output fixture-audit.json fixture `
  --data C:\game-data\Half-Life --runtime C:\source\build\runtime-x86-release\runtime\Release `
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
still confirm the actual loaded set. Production Windows audio remains native
DirectSound/WaveOut/Miles; `tests/sdl3/audio_stream_tests.cpp` uses a native SDL3
dummy device solely to qualify retained stream ownership/pause/cursor boundaries,
without claiming a native Linux/macOS runtime test.
