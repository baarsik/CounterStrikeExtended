# Metal renderer benchmark, 21 November 2026

The [fixed-demo protocol](metal-benchmark.md) was run on an Apple M5 Max with
macOS 27.2.1, a 2560×1440 borderless window on a 144 Hz display, VSync off,
and `-nosound`. Both signed app copies used the same game assets and the
60,019-frame `codex_longest_20261117.dem` fixture (SHA-256
`b50b9ed620032eb1a08014bbae0929f258d939bfd48d23fc34f85b8853f3298a`).
The baseline was revision `59022e1c` with the browser shutdown fix. The
candidate adds a compact Metal vertex stream for canonical single-texture
geometry. Every run exited with status zero; a final process check found no
game or CEF process from either staged app.

| Run | Baseline FPS | Candidate FPS | Baseline 1% low | Candidate 1% low | Baseline p95 | Candidate p95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 315.28 | 315.17 | 68.70 | 69.31 | 9.472 ms | 9.470 ms |
| B | 314.15 | 313.72 | 69.45 | 69.06 | 9.469 ms | 9.456 ms |
| Mean | 314.71 | 314.44 | 69.08 | 69.18 | 9.470 ms | 9.463 ms |

FPS and frame-interval changes are within run-to-run variation. This change
does not establish an FPS gain on this machine.

Separate full-demo runs with `CSE_METAL_FRAME_STATS` measured cumulative
counters, differenced per frame after the first 500 samples:

| Metal work per frame | Baseline | Candidate | Change |
| --- | ---: | ---: | ---: |
| Streamed vertex bytes | 1,081,519 | 817,438 | −24.4% |
| All upload bytes | 1,391,073 | 1,127,253 | −19.0% |
| CPU preparation | 0.344 ms | 0.344 ms | No measured change |
| Drawable acquisition | 1.456 ms | 1.468 ms | Within run variation |

The compact path preserves the existing 48-byte layout for noncanonical
homogeneous positions or texture coordinates. The renderer suite compares
the two layouts' pixels for equivalent coordinates and covers indexed
drawing, texture sampling, presentation, and resource lifetime. The remaining
large frame-time spikes are associated with drawable acquisition in this
borderless mode; reducing vertex uploads did not remove them.

Raw frame traces, per-run metadata, and paired analyzer JSON are in the local
ignored `build/metal-cef-20261120/` directory under `compact-*`,
`layout-profile-a/`, `compact-profile-a/`, and `comparison-compact-pair-*.json`.
