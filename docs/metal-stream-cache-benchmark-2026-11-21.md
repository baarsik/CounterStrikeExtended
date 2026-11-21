# Metal stream-cache benchmark, 21 November 2026

The [fixed-demo protocol](metal-benchmark.md) was run on an Apple M5 Max with
macOS 27.2.1, a 2560×1440 borderless window on a 144 Hz display, VSync off,
and `-nosound`. Both signed app copies used the same assets and the 60,019-frame
`codex_longest_20261117.dem` fixture (SHA-256
`b50b9ed620032eb1a08014bbae0929f258d939bfd48d23fc34f85b8853f3298a`).
The baseline was revision `be0d69c1`. The candidate releases stream buffers
unused for ten seconds, checking for cold buffers once per second. The launcher
and engine in each app were staged as a matching native cohort. Every valid
replay completed and exited with status zero; no staged game or CEF process
remained after the series.

Separate full-demo runs with `CSE_METAL_FRAME_STATS` measured retained buffer
bytes after each submission. The first 500 Metal samples were excluded:

| Metric | Baseline | Candidate | Change |
| --- | ---: | ---: | ---: |
| Mean retained stream buffers | 159.77 MiB | 90.90 MiB | −43.1% |
| Retained at replay end | 160.00 MiB | 86.56 MiB | −45.9% |
| Mean working stream buffers | 20.92 MiB | 21.53 MiB | Similar scene work |
| Total stream allocations | 25,762 | 25,847 | +85 |
| CPU preparation per frame | 0.343 ms | 0.346 ms | No established improvement |

The unprofiled replays used the analyzer's 59,476 measured frames per run:

| Comparison | Baseline FPS | Candidate FPS | Baseline 1% low | Candidate 1% low | Baseline p95 | Candidate p95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Alternating pair A | 321.41 | 322.29 | 69.88 | 69.93 | 9.249 ms | 9.320 ms |
| Alternating pair B | 320.20 | 314.72 | 69.87 | 69.91 | 9.331 ms | 9.451 ms |
| Quiet-load adjacent pair | 321.76 | 321.28 | 69.10 | 69.66 | 9.317 ms | 9.328 ms |

During the slower pair B, a Spotify renderer was observed using about one CPU
core. The following baseline recovered, and the final candidate run with that
process idle was within 0.15% of its adjacent baseline. These observations do
not establish an FPS gain or a reproducible FPS regression. The measured benefit
is lower retained Metal buffer memory. A discarded every-frame sweep variant
averaged 82.85 MiB retained but added work to each submission; the committed
variant checks once per second.

The renderer Release and sanitizer suites each passed 21/21 tests after this
change. A Half-Life `c1a0` smoke run with `-nosound` reached the map and `quit`
markers and exited zero. Raw frame traces, counters, metadata and comparisons
are retained in the local ignored `build/metal-cef-20261120/` directory under
`compact-profile-a/`, `sweep-profile-a/`, `sweep-unprofiled-*/`, and
`comparison-sweep-*.json`.

## Live map-transition validation

The final integrated macOS QA client joined a local Counter-Strike dedicated
server with `-nosound -metal` at 1280×720 borderless. The server changed from
`de_dust2` to `cs_office`, `de_inferno`, and back to `de_dust2` while the client
remained open. Its log recorded all four server generations and 4,298
connected game frames. CEF paint callbacks were observed before the first map
change; the bounded host trace filled after 2,048 events, so it does not prove
paint activity across all four maps.

The Metal diagnostic recorded 4,617 rows, 55 stream-buffer allocations and
three evictions, ending with 22.4 MiB of retained stream capacity. The client
then issued `quit`, exited zero, and left no game, browser host, or helper
process. This checks resource lifetime through map rebuilding, not an FPS
change. Server and game logs, traces, and metadata are under the local ignored
`build/metal-cef-20261120/final-mapcycle/` directory.
