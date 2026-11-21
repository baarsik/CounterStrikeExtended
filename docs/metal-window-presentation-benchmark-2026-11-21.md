# Metal window presentation benchmark, 21 November 2026

This experiment used revision `781a7d21` on an Apple M5 Max running macOS
27.2.1, with a 2560×1440, 144 Hz main display. A staged Counter-Strike app
replayed the same 7,102-frame `CSGO_MULTIHEAD1.dem` (SHA-256
`88a43ef6e2d2c6d6a8c9164c37bd6765d818c71ea2c93f5b14fb4748d3bbe647`).
Both 1280×720 runs used `-windowed -forceres -w 1280 -h 720 -novid -nosound
-metal`, `fps_override 1`, `fps_max 9999`, and `gl_vsync 0`. The second run
also used `-noborder`. No other game replay or build ran concurrently. Each
replay completed, exited zero, and left no staged game or CEF process.

The [fixed-demo analyzer](metal-benchmark.md) verified all 7,102 replay
frames, found 7,075 connected samples, excluded 500 warmup samples, and
required the remaining 6,575 frames to stay focused with the menu closed:

| Window at 1280×720 | Average FPS | 1% low FPS | Median interval | p95 interval | p99 interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| Titled | 159.62 | 56.36 | 5.277 ms | 11.983 ms | 13.843 ms |
| Borderless | 317.76 | 66.91 | 1.791 ms | 9.519 ms | 11.760 ms |

Separate replays with `CSE_METAL_FRAME_STATS` enabled localized the gap.
The counters are cumulative, so these values are frame-to-frame differences
after 500 warmup samples:

| Metal work per frame | Titled | Borderless |
| --- | ---: | ---: |
| Drawable acquisition, mean | 4.048 ms | 1.624 ms |
| Drawable acquisition, median | 3.184 ms | 0.007 ms |
| CPU preparation, mean | 0.398 ms | 0.310 ms |
| CPU encoding, mean | 0.188 ms | 0.203 ms |

A three-second live process sample of a separate borderless 2560×1440 replay
also found most sampled presentation time in `CAMetalLayer nextDrawable`.
The pinned SDL 3.4.20 Metal backend already disables `displaySyncEnabled` for
immediate presentation. An isolated build retaining the system's default
three-drawable pool in titled mode performed worse than the committed
two-drawable policy: 153.29 versus 159.86 FPS and 4.397 versus 4.048 ms mean
drawable wait in diagnostic replays. That experimental source change was
discarded.

This is a window-presentation finding, not an FPS improvement from the renderer
changes in this branch. The existing `-noborder` launch option gives a faster
measured path on this setup while preserving the selected 1280×720 resolution.
OpenGL at the same titled size measured 418.74 FPS in a separate replay; this
comparison does not establish on-screen presentation rate or visual latency.

## Borderless drawable-pool check

After the browser-paint changes, a separate signed-app A/B replay tested whether
the two-drawable policy should also apply to **borderless** immediate mode. The
candidate changed only that Metal policy and its matching launcher cohort. Both
apps used the same 1280×720 borderless demo, assets, `-nosound` arguments and
uncapped VSync-off settings. All four runs reported 7,102 demo frames, passed
the analyzer's focus and menu checks, exited zero, and left no game or browser
helper process.

| Pair | Default three drawables | Two drawables | Default p95 | Two-drawable p95 |
| --- | ---: | ---: | ---: | ---: |
| 1 | 317.23 FPS | 161.59 FPS | 9.498 ms | 11.853 ms |
| 2 | 316.23 FPS | 161.44 FPS | 9.608 ms | 11.846 ms |

The two-drawable borderless candidate lost about 49% FPS in each pair. It was
discarded; the committed policy still uses the system's default pool for
borderless windows and two drawables only for titled immediate mode. These
results also show why the titled result above should not be generalized to
other window modes.

The 60,019-frame `compact-profile-a` trace provides a timing cross-check for
the remaining borderless stalls. After warmup, frame interval and the
**previous** frame's drawable wait have correlation 0.891. Intervals of at
least 10 ms followed a previous drawable wait averaging 8.50 ms. The wait in
the same CSV row averages only 0.007 ms because the interval spans the prior
frame. This localizes the measured tail to presentation back pressure rather
than the 0.344 ms mean CPU preparation work; it does not identify a safe SDL or
Metal workaround by itself.

Raw traces and metadata are in the local ignored
`build/metal-cef-20261120/` directory under `titled-short-*`,
`borderless-small-short-*`, `pool2-borderless-*`,
`window-mode-short-comparison.json`, and `pool2-borderless-comparison-*.json`.
