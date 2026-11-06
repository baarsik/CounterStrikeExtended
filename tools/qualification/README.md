# Runtime qualification utilities

Run the Windows tools on VM1. `vm1.py` sends a local PowerShell file through
Parallels' UTF-16 encoded-command transport; it does not configure shares, change
isolation, or install dependencies. `--vm` defaults to `Windows 11`;
`--system` explicitly selects system transport instead of the current user.
Use `--interactive` for a short wrapper that invokes the probe on the logged-on
Windows desktop through Task Scheduler. The transport default timeout is 180
seconds; keep it above the probe timeout plus cleanup time.

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
Use a new testcase label or
evidence directory for each run. Source identity is supplied metadata, not an
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

The demo validator checks recorded structure/state. It does not replace runtime
playback, visual inspection, input/focus/device acceptance or provider auditing.
