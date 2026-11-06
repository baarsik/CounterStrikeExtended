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
