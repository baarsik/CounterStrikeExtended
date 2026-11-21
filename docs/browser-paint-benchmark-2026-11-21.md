# Browser paint decode benchmark, 21 November 2026

The [live browser paint pacing benchmark](browser-paint-pacing-benchmark-2026-11-21.md)
measures the effect of pumping complete images and sending their acknowledgements
within the game frame.

The browser host sends a full BGRA backing image with each paint message. The
game's decoder already owns the complete wire payload when it calls the VGUI
paint callback. The updated decoder passes a view into that payload instead of
allocating and copying another image. The callback consumes the pixels before
the message leaves `RunFrame`; the wire format and callback interface are
unchanged.

Reproduce the decode-only microbenchmark on macOS ARM64 from the repository
root:

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -Werror \
  -Isrc/engine/html/native tools/benchmark_browser_paint_decode.cpp \
  -o /tmp/cse-browser-paint-decode
/tmp/cse-browser-paint-decode
```

The fixture decodes a 1920×1080×4-byte image 300 times per trial. Four paired
trials alternate copy and borrowed-view order. Both paths check sampled pixels
and produce the same checksum. On the Apple M5 Max used for the Metal benchmark:

| Path | Median time per paint | Extra image allocation and copy |
| --- | ---: | ---: |
| Previous `Reader::Bytes` | 0.116508 ms | 8,294,400 bytes |
| Borrowed `Reader::BytesView` | 0.000077 ms | 0 bytes |

The individual copy trials were 0.158026, 0.122647, 0.110369 and 0.108988 ms;
the borrowed trials were 0.000099, 0.000078, 0.000076 and 0.000063 ms. Raw
output is retained locally in `build/metal-cef-20261120/browser-paint-benchmark/results.csv`.
This is a hot-cache decoder measurement. It does not include CEF rendering,
pipe transfer, Metal texture upload or browser frame scheduling, so it does
not establish an end-to-end browser FPS change.

The browser wire tests cover the borrowed pointer, empty and truncated byte
fields. Controller tests exercise full and partial paints, popup pixels and
the synchronous response callback. The callback implementations in both VGUI2
clients upload the texture before returning.

## macOS provider-fixture limit

The owned CEF provider fixture passed both write and read phases before the
decoder change, including paint, input, persistent storage and helper shutdown.
Later reruns on this macOS 27.2.1 machine stalled before CEF's first `OnPaint`.
The same failure occurred with the earlier host executable and with the updated
controller, so the borrowed-view callback was not reached in those failing
runs. The host's frame trace reached browser creation, page load, resize and
invalidate, then timed out at 30 seconds. CEF also logged a macOS process
signature-validation error (`-67030`), although the signed app verified on
disk and `codesign -v <live-host-pid>` reported the host dynamically valid.
Launching through the physical host path instead of the runtime symlink did
not change the result. A standalone pinned-CEF SDK probe also missed its paint
deadline in this session, but exited before making any HTTP request, so it does
not isolate the host's post-load failure. An experimental external begin-frame
probe did not help and was removed.

The pinned CEF SDK's full `cefclient` sample was also built and run in
off-screen mode on this machine. It visibly rendered a local green test page,
while logging the same `-67030` signature warning. The warning alone therefore
does not explain the host's missing frame. Isolated host experiments that made
the native parent visible and active, removed `no-startup-window`, disabled the
sandbox, or started at 192×96 instead of 1×1 still loaded the fixture page
without delivering paint. A diagnostic callback confirmed that neither
`OnPaint` nor `OnAcceleratedPaint` was delivered. All experimental source
changes were removed, and the production host was rebuilt.

These observations do not establish why that standalone fixture stopped painting. The local
ignored evidence is under `build/metal-cef-20261120/cef-provider-fixture/`,
`cef-verify-fixture/`, `cef-canonical-fixture/`, and
`cef-sdk-correct-name-fixture/`; the SDK sample screenshot is under
`cefclient-osr-diagnostic/`.

## Joined-game paint and quit validation

A loopback Counter-Strike dedicated server on `de_dust2` supplied its normal
MOTD to a macOS Metal client launched with `-nosound`. The client joined
`127.0.0.1:27550`, displayed the HTML MOTD over the game, waited about ten
seconds, then ran the console `quit` command. The owned host's frame trace
recorded a 546×324 view, `OnPaint` callbacks, paint messages, and browser
closure. The visible MOTD confirms the callback reached the VGUI texture path.
The same CEF `-67030` warning appeared in these successful game runs, so that
warning is not by itself evidence of paint failure.

Six repeat joined-client runs all connected, delivered 5–7 `OnPaint` callbacks,
closed their browser, exited with code zero, and left no game or CEF helper
process. Their elapsed times were 13.665–13.782 seconds. In three further
runs, the host process was stopped with `SIGSTOP` six seconds after client
launch, while the MOTD was active. The game's `quit` still exited with code
zero in 28.665–28.674 seconds and left no game or helper process. A stopped
host cannot flush its trace, so the stopped runs establish bounded teardown,
not paint delivery. Raw traces, logs, a screenshot, and the sweep table remain
in the ignored `build/metal-cef-20261120/motd-game-quick/` and
`motd-shutdown-sweep/` directories.

This validates live painting and the decoder callback path in the game. The
joined-game run did not measure browser frame rate or compare previous and
updated clients under identical MOTD workload.

The Windows x64 native process fixture was rebuilt from the final branch's
`browser_wire.h`, `process_channel.cpp`, and `process_channel.h` with MSVC
`/W4 /WX`, then passed. It covers duplex message ordering, queued-command
drain on shutdown, abnormal and truncated child output, and bounded reap of
three stalled-child modes. This checks the current Windows channel and quit
code path at fixture level; Windows gameplay and CEF rendering were not run.

## Host paint encode benchmark

After the joined-game validation, the CEF host was changed to write its
retained view and popup images directly as wire segments. The previous
`Writer::Bytes` path allocated a second complete payload and copied both
images before writing them. The encoded header, field order, lengths and
image bytes remain identical, and the retained CEF backing image is still
copied from the callback before it can be sent or reused. The later backing
copy change below narrows that copy for small dirty rectangles.

Run the encode-only comparison from the repository root:

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -Werror \
  -Isrc/engine/html/native tools/benchmark_browser_paint_encode.cpp \
  -o /tmp/cse-browser-paint-encode
/tmp/cse-browser-paint-encode
```

Each path encodes 300 paints per trial, with four alternating paired trials
and matching output checksums. The smaller size is the 546×324 MOTD view
observed in the live Metal game. On the same M5 Max:

| View | Previous full payload copy | Segmented metadata | Extra allocation and copy removed |
| --- | ---: | ---: | ---: |
| 546×324 | 0.027151 ms/paint | 0.000021 ms/paint | 707,616 bytes/paint |
| 1920×1080 | 0.220193 ms/paint | 0.000063 ms/paint | 8,294,400 bytes/paint |

These are medians of hot-cache CPU encoding trials, excluding the CEF
callback, pipe write, engine decode and texture upload. Raw output is in
`build/metal-cef-20261120/browser-paint-encode-results.csv`. The browser
paint fixture compares segmented payload bytes with the former `Writer`
format for both view and popup pixels. The test passed on macOS ARM64 in
Release and under sanitizers and on Windows x64 with MSVC `/W4 /WX`.
A staged `-nosound -metal` joined-game run with the rebuilt host loaded the
MOTD, recorded two `OnPaint` callbacks, closed the browser, exited zero, and
left no CEF helper process. The encode comparison does not establish an
end-to-end browser frame-rate gain.

## Host backing copy benchmark

The pinned CEF 156 `cef_render_handler.h` says `OnPaint` supplies the entire
BGRA image plus rectangles that need repainting. A temporary trace in an
isolated QA host found 574 of 586 live animated MOTD paints marked about 35%
of a 728×432 view dirty. The trace-only instrumentation was removed before
building the candidate. For one clipped rectangle covering less than half the
existing image, the host now copies only those rows into its complete retained
backing. The first paint, resized image, empty or invalid damage, multiple
rectangles, and larger damage still use the full copy. The wire payload and
paint acknowledgement remain unchanged.

Reproduce the CPU copy benchmark from the repository root:

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -Werror \
  -Isrc/engine/html/native tools/benchmark_browser_paint_backing.cpp \
  -o /tmp/cse-browser-paint-backing
/tmp/cse-browser-paint-backing
```

Four alternating paired trials copy a 728×432 image 10,000 times per trial,
with one 500×220 dirty rectangle and matching final images and checksums.
On the same M5 Max, median time per paint was 0.013304 ms for the complete
1,257,984-byte copy and 0.009155 ms for 440,000 dirty bytes, a 31.2% copy-time
reduction in this hot-cache microbenchmark. The raw CSV is under the ignored
`build/metal-cef-20261120/cef-dirty-probe/` directory. This excludes CEF
rendering, pipe transfer, the engine decoder and the Metal texture upload.

Four alternating joined-game runs compared a baseline full-copy host and the
partial-copy host with identical game binaries and assets, at 2560×1440
borderless Metal, `-nosound`, and a 60 FPS cap. Both hosts saw about 60 CEF
`OnPaint` callbacks per second. The candidate's 585 and 585 callbacks compare
with 584 and 585 for the baseline; all four games retained a 16.535 ms p95
frame interval, exited zero, and left no game or helper process. These runs
show no browser-cadence or capped-game regression, and no demonstrated game
FPS gain. A native paint test verifies retained pixels for clipped, resized,
missing, invalid and multi-rectangle damage. Release and sanitizer browser
suites passed 6/6; the paint fixture also passed on Windows x64 under MSVC
`/W4 /WX`. Two further candidate clients exited zero in 29.724 and
29.735 seconds after their active CEF host was stopped with `SIGSTOP`, with no
remaining game or helper process. The local live traces and screenshot are
under `build/metal-cef-20261120/cef-partial-copy-live/` and
`cef-partial-copy-stopped/`.
