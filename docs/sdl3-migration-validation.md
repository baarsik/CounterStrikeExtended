# SDL3 migration validation

Native SDL3 3.4.20 replaces SDL2 throughout retained source, builds and the audited
Windows x86 runtime. No SDL2 compatibility runtime is used. Historical manifests
and unmodified upstream header text remain archival material.

The validated product snapshot is `c4f729d`, including the cursor-ownership fix.
Its production SDK tree is `8cb90868bd5e6e95026dbc0a2fa4c4cd983554d5`; CMake tree is
`e9623cb1b61a0254c3f772259a0fa963aff9ce0a`. Results below apply to that snapshot;
the later source cleanup and Miles removal are covered by
[music/runtime validation](mpg123-migration-validation.md). The SDL2 reference
uses repaired baseline `23a2135` and SDL2 2.0.20.

## Validation

VM1 ran Windows ARM64 build 26300 with 12 vCPUs and 20 GB RAM, executing Windows
x86 binaries under emulation. Toolchain: Visual Studio 2026, MSVC v145
19.51.36260.0, Windows SDK 10.0.26100.0 and CMake 4.4.4. Warnings, assertions,
floating-point settings and provider requirements were retained.

| Check | Result |
| --- | --- |
| Fresh builds | All four hardware/runtime Release/Debug presets passed; explicit Release A3D and broadcasting builds also passed. |
| Dependency closure | All 94 pinned SDL3 files validated; the 43-binary x86 package resolves formal dependencies without SDL2 or SDL2-compat. That snapshot included Miles, Steam, browser and codec providers. |
| Source-built outputs | All 16 required staged outputs and 12 observed source-built modules match their manifests in each gameplay run; live snapshots contain only pinned SDL3. |
| CS and HL gameplay | Release and Debug each recorded 651 alive network samples and 301 movement-command samples, replayed to full EOF and exited cleanly. |
| Rendering and media | Textured world/HUD captures passed at 640×480 and desktop fullscreen; actual SDL3 startup-video rendering was observed. Repeated native DirectSound captures were nonzero. |
| Input and focus | Real console ASCII, raw/legacy switching, menu opening, typed quit and two Alt-Tab away/back cycles passed. Native fixtures cover keyboard layouts, Unicode conversion, fractional input, cursor ownership and retained audio-stream policy. |
| Dedicated servers | CS and HL localhost startup, map/status and clean shutdown passed. |
| Automated checks and review | 61 Python tests passed, generated targets remained synchronized, and final independent review had no open material findings. |

See [build instructions](building.md) and [qualification tools](../tools/qualification/README.md)
for commands. SDL audio dummy-device tests cover retained stream policy and do
not establish native Linux/macOS runtime behavior.

## Release benchmarks

Each game used the same fixed recording, assets, providers and ASCII settings for
both versions: 640×480 windowed, `-forceres`, two-player mode and vsync disabled.
One warmup per version was excluded, followed by seven alternating measured pairs
per game: 32 completed runs total. Every run passed full EOF, clean exit and owned
foreground-window checks. Binary hashes match the qualified reference and candidate.

| Game | SDL2 median FPS | SDL3 median FPS | Median paired FPS change | Paired change range |
| --- | ---: | ---: | ---: | ---: |
| Counter-Strike | 177.274 | 177.026 | −0.234% | −17.575% to +14.725% |
| Half-Life | 272.810 | 266.452 | −3.674% | −11.223% to +18.817% |

Paired changes are calculated before taking their median and need not equal the
ratio of the independent FPS medians. Variation is substantial; these instrumented
VM observations do not establish statistical significance or native-hardware performance.

The following are median per-run engine-profile quantile bounds in milliseconds,
rounded outward to 0.001 ms, rather than pooled percentiles or presentation timings.

| Game / version | p50 | p95 | p99 |
| --- | ---: | ---: | ---: |
| CS SDL2 | 3.558–3.572 | 4.385–4.406 | 21.739–22.223 |
| CS SDL3 | 3.546–3.559 | 4.464–4.485 | 21.739–22.223 |
| HL SDL2 | 3.236–3.247 | 3.773–3.788 | 8.333–8.404 |
| HL SDL3 | 3.184–3.195 | 3.802–3.817 | 9.259–9.346 |

Integer `host_speeds` FPS values provide timing bounds, not precise frame traces.
Profiles include loading/sign-on and logging overhead. CS retained five SDL2 and
six SDL3 zero-FPS frames with unbounded maximum duration among 4,669 profiles per
version; HL had none among 4,585. No measured run contained the 999 sentinel.

## Coverage limits

- Exclusive/no-FBO mode and unsupported-size fallback completed replay and clean
  shutdown with matching mode selection. Both SDL2 and SDL3 exclusive captures
  were corrupt internally and black through the VM viewer; actual exclusive
  visual output remains unverified.
- Clipboard, non-ASCII game-UI entry, double-click, minimize, physical mouse deltas,
  device hotplug, gamepads, high DPI, hardware refresh/gamma and the live VGUI2-to-VGUI1
  cursor handoff remain unverified beyond source review and applicable helper tests.
- Native Linux/macOS and inactive software-client paths have no runtime acceptance.
  Static closure and sampled modules do not prove every later dynamic feature.

## Evidence and environment

Raw logs, recordings, hashes, captures and review reports remain outside the repository:

```text
/Users/mmalakhouski/.codex/artifacts/cse-sdl3-assessment-20261106/migration
```

Key records are `final2-product-source-manifest.json`, `final2-fresh-build-results.json`,
`final2-fresh-optional-result.json`, `final2-fixture-audit.json`,
`final2-runtime-verification.json`, `benchmark-final2-analysis.json`,
`benchmark-final2-summary.json` and `benchmark-final2-pairs.csv`.
Failed diagnostic attempts remain in that archive and are excluded from passing
qualification results.

VM1's installed toolchain and build sandboxes are retained. Temporary test tasks
and host shares were removed; host sharing is off and VM isolation is restored.
