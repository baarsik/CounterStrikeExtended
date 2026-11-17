# Metal renderer qualification

These fixtures exercise the production CPU preparation, recorder/backend and actual
SDL_GPU `metal` device. They do not replace the historical bounded prototype tests.
Run from the repository root on macOS 14+ ARM64 with the repository's pinned, source-built SDL3:

```sh
cmake -S tests/native/metal_renderer -B build/metal-renderer-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_METAL_SDL_LIBRARY="$PWD/build/runtime-macos-arm64-release/runtime/Release/libSDL3.0.dylib" \
  -DCSE_METAL_COMPILER="$(xcrun --toolchain Metal --find metal)" \
  -DCSE_METAL_LINKER="$(xcrun --toolchain Metal --find metallib)"
cmake --build build/metal-renderer-debug --parallel
ctest --test-dir build/metal-renderer-debug --output-on-failure
```

Use a separate build directory with `-DCSE_METAL_SANITIZERS=ON` for ASan/UBSan.
Pixel fixtures require a real logged-in display/GPU session. The two parity tests
create an isolated OpenGL oracle context as well as a Metal device; the engine's
Metal-only no-GL requirement is verified separately in actual-game audits.

| Fixture | Evidence |
| --- | --- |
| `metal-draw-preparation` | Matrices, indexed topology, provoking vertices, texture conversion and uniforms |
| `metal-session-device` | Actual render/readback, resident upload reuse, exact mesh/immediate color/scroll/detail/fog/blend/wire output, indexed offsets, adjacent draw coalescing with uniform/texture/scissor barriers and transparent order, texture versions, palette enable, incomplete textures, sampler min/mag, explicit four-tap border anisotropy, clears, viewport mapping and failure retirement |
| `metal-mesh-pages` | Actual page rollover, bounded 128 MiB residency, equivalent streaming under cache pressure, sole-page retirement during pending work and balanced GPU buffer ownership |
| `renderer-backend-lifecycle` | Startup upload, flush/reopen, retained presentation capture across pending window resizes, absent-presentation handling, real-window resize/minimize/restore, teardown and sticky errors |
| `renderer-backend-large-precache` | 768 MiB of retained source textures through the production backend, followed by first-use rendering of early and late uploads |
| `renderer-backend-ram-budget` | Runtime budget selection below, at and above 8 GiB of installed RAM, including unavailable RAM information |
| `renderer-backend-device-mode` | Actual SDL_GPU device creation enables validation only in Debug, independently of NDEBUG |
| `renderer-selection` | Default/override/conflict policy, Metal-disabled and non-Apple behavior |
| `radar-public-release` | Actual engine interface factory/table callback: null, half-null, stale and current-unavailable handles |
| `metal-radar-effects` | Layer depth/order, independent base/lightmap modulation, tone, texture lifetime, generated-image retirement, aggregate map-index/per-draw expansion bounds, glyph shadow, pre-HUD blur and odd resize |
| `metal-lifetime-names` | 131,072 generations with one live reservation, all 65,535 simultaneous names, explicit exhaustion and reuse |
| `metal-lifetime-session` | 96 completed NPOT updates under a three-image source budget, exact retained bytes, persistent live-cache reuse and no image reupload after redundant sampler parameters |
| `metal-lifetime-radar` | 96 register/upload/delete cycles without querying stale handles, exact source reclamation and balanced actual SDL GPU ownership |
| `metal-lifetime-radar-records` | 96 abandoned identities with counted weak control blocks, proving stale registry records are erased at frame sweeps |
| `metal-lifetime-queued-reuse` | Deleted/reused numeric name, distinct generations/service handles, independent old/new queued pixels and final source retirement |
| `radar-gl-metal-parity` | 94 actual GL/Metal full-image comparisons: all eleven primitives, player direction/offscreen transitions, clipping/fade, mask/ring alpha comparisons, gamma, generated premultiplied AA edges/color ties and both tone modes |
| `metal-blur-oracle` | Actual 33-tap radar and five-tap Smoked filters versus independent double-precision separable convolution with RGBA8 intermediate quantization |
| `legacy-gl-metal-parity` | 303 actual GL/Metal cases: fog, alpha comparisons, blend factors, flat provoking colors, overlapping viewports, incomplete images, texture formats/environments/wrap and all six min filters over twelve scales |

The legacy parity limit is one byte per RGBA8 component. Radar parity and blur allow
two bytes for interpolation, shader arithmetic and attachment-rounding differences.
These limits apply to deterministic fixtures, not to unsynchronized screenshots of
animated gameplay. The blur oracle excludes only the outer two-pixel analytic mask;
it compares the full interior horizontal and vertical transitions. Assertions remain
enabled in Release.

The runtime recorder allows up to 1 GiB of retained CPU texture images on systems with
8 GiB of installed RAM or less, and 2 GiB on systems with more than 8 GiB. If installed
RAM cannot be determined, the limit is 1 GiB. Storage is allocated on demand.
Custom server model precaches can exceed 650 MiB before world rendering begins.
This limit is separate from the 256 MiB evictable GPU texture cache; per-upload bounds
and the recorder's accounting for queued image versions remain unchanged.

## Mip boundary behavior

The custom legacy border path uses the device's unclamped LOD for min/mag selection
and clamped LOD for mip selection. Using analytic `log2` and rounding disagreed with
both Apple GL and Metal's ordinary sampler around the nearest-mip transition at scale
1.42 on the qualified machine. The maintained sweep includes 1.40, 1.42–1.46 and 1.50,
plus magnification and stronger minification. Unclamped-only and clamped-only substitutions
were both rejected by actual comparisons; clamped zero LOD alone wrongly selects the
magnification filter for non-mipmapped minification.

The retained min/mag split at zero matches the measured supported Apple driver across
these cases; changing it to 0.5 from the GL specification's special filter rule would
change those pixels. This is measured driver equivalence, not a claim that all GL drivers
share that approximation. Explicit multi-tap anisotropic border reconstruction retains
its reduced-footprint calculation. A separate numeric fixture verifies four-tap integration;
this is not an exhaustive hardware-anisotropy equivalence sweep.
Apple's [LOD-query documentation](https://developer.apple.com/documentation/metal/predicting-which-mips-the-gpu-samples-with-level-of-detail-queries)
requires Apple7/Mac2 capability; M1 and later supported Apple Silicon meet that floor.

## Qualification scope

The November 2026 acceptance machine used Apple M5 Max. Production Release/Debug Metal
clients, an OpenGL-only build and the suites above were exercised. Actual copied-app
validation covers Counter-Strike and Half-Life rendering, startup video, screenshots,
radar/UI combinations and representative lifecycle paths. Runtime captures use `-nosound`
because an existing host CoreAudio semaphore stall was observed under the debugger;
audio/debugger compatibility is not established by renderer acceptance.

Windows x86/x64 keep-going runtime builds compiled affected engine/launcher owners and
removed the newly introduced diagnostics. Other affected client owners retained only
their baseline diagnostics. Full Windows builds remain blocked by pre-existing failures; this is not a complete Windows runtime-build or gameplay pass.
No dependency pins or public game/network/save/demo formats change.

Controlled runtime timing is a bounded same-machine comparison. `CSE_FRAME_TIME_LOG`
records frame intervals and CPU phases; `graphics_ms` is CPU time spent in the engine's
graphics phase, not GPU execution time. It does not establish a general FPS improvement.
Private captures, logs and machine-specific provider/asset paths remain under ignored `build/`.


The bounded acceptance replay used the same locally recorded stationary world/viewmodel
and Smoked scoreboard at an effective 1280×1024, `fps_max 100`, `gl_vsync 0`, default
MSAA and `-nosound` on macOS 27.0.1. Runs were ordered default Metal, OpenGL, OpenGL,
explicit Metal. Each selected window covers active seconds 6–11 after an early capture;
all selected frames were focused. The recorded live scene had health/radar, but replay
on both backends showed an incomplete HUD and spectator scoreboard. These measurements
therefore cover world/Smoked rendering, not live radar or full gameplay performance.

| Run | Frames | Frame interval p50 / p95 (ms) | Graphics CPU p50 / p95 (ms) |
| --- | ---: | ---: | ---: |
| metal-a | 502 | 9.953 / 9.956 | 0.711 / 0.906 |
| gl-a | 500 | 9.954 / 9.957 | 0.810 / 2.727 |
| gl-b | 502 | 9.953 / 9.956 | 0.776 / 3.174 |
| metal-b | 501 | 9.954 / 9.957 | 0.741 / 0.859 |

Frame intervals remained near the configured cap; the CPU-phase differences do not
establish higher achievable FPS or GPU execution speed. Whole-process peak RSS was
474 MiB for both Metal replays and 523–524 MiB for OpenGL. A separate live Metal map
cycle peaked at 647 MiB and exited normally. RSS was sampled every 0.5 seconds and
excludes separate helper processes; it is not GPU residency or a long-duration leak test.

## Uncapped performance regression check

The November 17, 2026 follow-up used a recorded multiplayer demo on the same M5 Max
at 2560×1440, default MSAA, VSync disabled and `-nosound`. Timedemo bypassed the FPS
cap. Each row measures connected frames 500–2499 of the same 3,000-frame replay;
all 2,000 selected frames were focused. These frame intervals include CPU and GPU
waiting; they are not isolated GPU timings.

| Warm replay | Frame interval p50 / p95 (ms) |
| --- | ---: |
| Original Metal Release | 3.674 / 5.340 |
| Updated Metal Release A | 3.085 / 5.578 |
| Updated Metal Release B | 3.154 / 5.256 |
| OpenGL | 1.503 / 3.438 |

The update selects GPU validation explicitly from the CMake configuration, copies
filled smooth triangle lists directly, coalesces adjacent draws with identical GPU
state, and preserves uploaded image identities across redundant sampler updates.
Median Metal frame time fell by 14–16% in these warm repeats. Tail latency did not
consistently improve, and OpenGL remained faster. This bounded replay does not
establish performance parity or predict the improvement on an M1 or another server.

## Native mesh and quad submissions

The subsequent native path keeps immutable world/brush polygons in bounded GPU arena
pages and submits their material/transform snapshots directly. Studio submissions retain
unique vertices and uint32 indices; flat/wire modes retain their equivalent lowering.
Built-in textured UI and glyph quads bypass individual vertex commands. Specialized
water/sky/effects and external legacy callers retain their compatible rendering paths.

Per-draw entity color and scrolling precede the existing texture matrices. Lightmap
updates, detail textures, transparency order and polygon boundary wireframe are retained.
CPU source geometry and resident GPU mesh pages each have a 128 MiB cache bound. A full
cache uses the same streamed geometry/shaders, without dropping surfaces or changing
quality. Map/model clears and lightmap rebuilds retire ownership; queued frames and
submitted SDL_GPU commands retain the resources they still need.

The final same-machine 2560×1440 replay compared against local checkpoint `cd2ed9cd`,
which already includes the earlier Release-device/submission fixes. Default 4x MSAA,
filtering, effects and VSync-off settings were identical. After a warm-up, runs alternated
checkpoint/native/checkpoint/native. Each row contains connected completed frames
500–2499, all 2,000 focused, with `-nosound` and the same local demo/assets.

| Replay | Frame interval p50 / p95 / mean (ms) |
| --- | ---: |
| Checkpoint A | 3.124 / 5.312 / 3.447 |
| Native A | 2.504 / 6.304 / 3.138 |
| Checkpoint B | 3.166 / 5.386 / 3.479 |
| Native B | 2.488 / 6.404 / 3.125 |

Median FPS increased by 25–27%; mean replay throughput increased by 10–11%. Tail frame
times regressed, so this is a throughput improvement, not an across-the-board frame-pacing
improvement or OpenGL parity claim. No graphics setting, feature or pixel tolerance was
reduced. These measurements are from M5 Max; M1 performance has not been measured.

The final native fixtures passed 18/18 in Release and Debug ASan/UBSan, including real
GPU page rollover, budget fallback and queued page retirement. Shared recorder suites
passed 22/22 with and without ASan/UBSan, and dispatch passed 48/48. The macOS ARM64
launcher/runtime built; changed OpenGL-only owners passed a separate syntax check.
Independent source review found no actionable defects. Actual local Counter-Strike and
Half-Life runs cover screenshots and map changes, with Original/Smoked UI and classic/
advanced radar exercised in Counter-Strike. This is bounded coverage, not an exhaustive
feature or M1/Windows execution qualification. Per-run logs, runtime cohorts, CSVs and
captures are retained under ignored `build/native-metal-20261117/`.
