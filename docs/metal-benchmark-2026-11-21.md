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

## Final integrated build check

After the Metal stream-cache and CEF changes on this branch, the signed final
QA app replayed the same 60,019-frame demo twice at 2560×1440 borderless,
uncapped VSync-off Metal, with `-nosound` and no local server. The complete
Release engine, launcher, and browser host came from the committed tree at
`f9e3635d`. The [fixed-demo analyzer](metal-benchmark.md) accepted 59,976
connected frames and 59,476 post-warmup frames in each run, with no focus or
menu contamination:

| Final run | Average FPS | 1% low FPS | Median interval | p95 interval | p99 interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 335.22 | 74.74 | 2.039 ms | 8.451 ms | 10.960 ms |
| 2 | 334.28 | 74.37 | 2.055 ms | 8.502 ms | 10.981 ms |

Both demo replays reported all 60,019 frames, ended through `quit` with exit
code zero, and left no game or CEF helper process. These are integrated-build
repeatability results, not a controlled FPS comparison with the earlier
compact-vertex baseline above. Raw frame logs, process and binary hashes, and
analyzer JSON are in the local ignored `build/metal-cef-20261120/final-full-demo/`
directory.

A third replay with `CSE_METAL_FRAME_STATS` enabled also exited zero after the
complete demo. It is a diagnostic run, so its FPS is excluded from the table.
After 500 warmup samples, CPU preparation averaged 0.347 ms per frame and
drawable acquisition averaged 1.263 ms (6.768 ms p95); no GPU fence wait was
recorded. Retained stream capacity fell from 153.2 MiB at the first measured
frame to 91.0 MiB at exit, with a 160 MiB observed maximum. The sampled game
process RSS was 1705–1716 MiB from 20 to 180 seconds; that figure includes the
whole game process, not just Metal resources. The trace and memory samples are
in the local ignored `build/metal-cef-20261120/final-metal-profile/` directory.

The shared runtime also loaded Half-Life `c1a0` in a separate `-nosound -metal`
smoke run. The log recorded `Spawn Server c1a0` and the scripted `quit` marker;
496 frame-log rows were written, the process exited zero, and no game or CEF
helper remained. Its log and metadata are under the local ignored
`build/metal-cef-20261120/final-half-life-smoke/` directory.

A separate `-nosound -metal` Half-Life multiplayer smoke used the same final
engine and joined a local `crossfire` dedicated server. The frame log recorded
1,062 connected frames, then the scripted `quit` completed with exit code zero
and left no game or CEF helper process. The browser provider initialized, but
this normal Half-Life MOTD did not create a CEF browser; the run validates the
shared engine and renderer path, not Half-Life browser painting. Its logs and
metadata are under the local ignored
`build/metal-cef-20261120/final-half-life-motd/` directory.

## OpenGL reference on the final integrated build

The same signed QA app, assets, demo, 2560×1440 borderless window, VSync-off
setting, and `-nosound` launch were used for two complete OpenGL replays. The
only renderer flag changed from `-metal` to `-gl`. Each run reported all 60,019
demo frames, passed the same analyzer filters, exited zero, and left no game or
CEF process. The Metal runs above preceded these OpenGL runs, so the comparison
is a reference for this machine rather than an interleaved renderer trial.

| Run | Metal average FPS | OpenGL average FPS | Metal 1% low | OpenGL 1% low | Metal p95 | OpenGL p95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 335.22 | 331.95 | 74.74 | 106.58 | 8.451 ms | 5.237 ms |
| 2 | 334.28 | 333.54 | 74.37 | 106.08 | 8.502 ms | 5.167 ms |

Metal's mean FPS was slightly higher, while its lower 1% low FPS and higher
p95 frame interval show a repeatable tail-latency gap. The separate Metal diagnostic
trace above attributes most of its measured presentation time to drawable
acquisition. This renderer comparison does not isolate the OS compositor or
establish that the compact vertex and cache changes improved FPS. OpenGL traces,
process checks, and analyzer JSON are under the local ignored
`build/metal-cef-20261120/final-gl-demo/` directory.

## Multisample workload diagnostic

Two more complete replays used the same final Metal app, demo, settings, and
borderless window, adding only `-nomsaa` to select one color/depth sample in
place of the default 4× MSAA. This changes image quality, so it is a diagnostic
comparison rather than a proposed default. All four runs below reported the
60,019 demo frames, passed the analyzer's focus and menu checks, exited zero,
and left no game or CEF process.

| Run | Default 4× MSAA FPS | One-sample FPS | Default p95 | One-sample p95 |
| --- | ---: | ---: | ---: | ---: |
| 1 | 335.22 | 323.67 | 8.451 ms | 8.966 ms |
| 2 | 334.28 | 328.40 | 8.502 ms | 8.845 ms |

Removing multisampling did not improve the mean or tail frame intervals on
this setup. The single-sample presentation path also copies to a separate
retained final-color target before the swapchain blit, whereas the matching
4× MSAA resolve can be presented directly at this size. That code-path
difference is a plausible contributor, not an isolated cause. The tests were
not interleaved, and no rendering behavior was changed. Raw results are under
the local ignored `build/metal-cef-20261120/final-nomsaa-demo/` directory.

## Frames-in-flight diagnostic

SDL Metal defaults to two permitted frames in flight. A separate, signed app
copy temporarily requested three after device creation. It replayed the same
60,019-frame demo with `-nosound -metal`, the same borderless window and
settings, and both frame and Metal diagnostic traces enabled. The complete
replay exited zero with no game or CEF process left. Against the diagnostic
two-frame run above, the measured average was 333.90 versus 333.75 FPS,
1% low was 74.87 versus 74.29 FPS, and p95 interval was 8.511 versus
8.575 ms (two versus three frames). Drawable acquisition p95 was 6.768 versus
6.772 ms. A single ordered pair cannot resolve very small differences, but
there was no useful improvement in the measured tail. A larger in-flight
limit can also increase visual latency. The temporary source change was
reverted; the final renderer keeps SDL's two-frame default. Raw diagnostic
traces and analyzer output are in the ignored
`build/metal-cef-20261120/flight-three-probe/` directory.
