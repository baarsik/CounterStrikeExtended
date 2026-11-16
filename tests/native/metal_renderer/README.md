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
| `metal-draw-preparation` | Matrices, topology, provoking vertices, texture conversion and uniforms |
| `metal-session-device` | Actual render/readback, texture versions, palette enable, incomplete textures, sampler min/mag, explicit four-tap border anisotropy, clears, viewport mapping and failure retirement |
| `renderer-backend-lifecycle` | Startup upload, flush/reopen, retained presentation capture across pending window resizes, absent-presentation handling, real-window resize/minimize/restore, teardown and sticky errors |
| `renderer-backend-large-precache` | 768 MiB of retained source textures through the production backend, followed by first-use rendering of early and late uploads |
| `renderer-selection` | Default/override/conflict policy, Metal-disabled and non-Apple behavior |
| `radar-public-release` | Actual engine interface factory/table callback: null, half-null, stale and current-unavailable handles |
| `metal-radar-effects` | Layer depth/order, independent base/lightmap modulation, tone, texture lifetime, generated-image retirement, aggregate map-index/per-draw expansion bounds, glyph shadow, pre-HUD blur and odd resize |
| `metal-lifetime-names` | 131,072 generations with one live reservation, all 65,535 simultaneous names, explicit exhaustion and reuse |
| `metal-lifetime-session` | 96 completed NPOT updates under a three-image source budget, exact retained bytes and persistent live-cache reuse |
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

The runtime recorder allows up to 1 GiB of retained CPU texture images, allocated on
demand. Custom server model precaches can exceed 650 MiB before world rendering begins.
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
