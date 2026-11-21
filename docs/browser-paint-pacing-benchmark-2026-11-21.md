# Browser paint acknowledgement benchmark, 21 November 2026

A live Counter-Strike MOTD exposed a browser throughput limit after CEF itself
had already rendered a frame. The game read each large pipe message in pieces.
When a read found no new bytes before the host resumed writing, the game stopped
pumping until its next frame. After the VGUI callback, it also waited until the
next frame to send the paint acknowledgement. CEF coalesces new backing images
while an acknowledgement is outstanding.

The updated controller yields when the current frame has made pipe progress but
is still inside a message, within its existing four-millisecond pump budget.
It also flushes queued acknowledgement bytes immediately after synchronous
VGUI callbacks, with a fixed limit on writes per frame. The process protocol,
CEF frame rate, texture callback, and shutdown timeout are unchanged.

## Reproduce

Use two isolated, signed macOS ARM64 game apps with identical assets and CEF
host binaries. The baseline is the browser controller at `d4cd659f`; the
candidate is the controller change described here. On the Apple M5 Max used
for this test, both staged host executables had SHA-256
`53d3bbed8507c800b3b293b0f1411387bb36e81bee5c2ddab023d236b72482ab`.
The system ran macOS 27.2.1 with a 2560×1440, 144 Hz display.

Copy [the 522-byte animated MOTD fixture](../tests/native/cef_host/fixtures/animated_motd.html)
into a disposable dedicated server's `cstrike/cse_motd_benchmark.txt`. Start a
loopback server on `de_dust2` with `-nosound`, `+sv_lan 1`, and
`+motdfile cse_motd_benchmark.txt`. For this local test the dedicated server
needed `SteamAppId=10 SteamGameId=10` in its environment. The 522-byte file is
below the server's 1,535-byte MOTD limit.

For each client app, make `cstrike/cse_motd_animated.cfg` contain
`fps_override 1`, `fps_max 60`, `connect 127.0.0.1:27551`, 650 separate
`wait` commands, and `quit`, in that order. Launch at a titled 1280×720
window with `-nosound -metal +exec cse_motd_animated.cfg`. Set
`CSE_CEF_FRAME_TRACE` and `CSE_FRAME_TIME_LOG` to separate absolute CSV paths
for every run. Alternate candidate, baseline, baseline, candidate. Wait for
each process to exit and check for remaining game and CEF helper processes.

The host trace writes on clean CEF shutdown. For every `on_paint` event, the
immediately following `paint` event's `second` field records whether the
previous backing image was still in flight (`1`) or free (`0`). Require the
trace's `closed` event so the bounded recorder was not cut off. In the game
frame CSV, compare `graphics_ms` and `interval_ms` only for connected frames.
The local raw traces, logs, screenshot and paired result table are under the
ignored `build/metal-cef-20261120/browser-pump-isolated/` directory.

## Results

Both modes received about 59–60 CEF `OnPaint` callbacks per second. The
baseline's browser was still waiting for an acknowledgement at almost every
callback. The updated controller had a free paint slot at most callbacks:

| Mode | Callback count per run | Callbacks with paint in flight | Callbacks with free paint slot | Median graphics work | p95 game interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| Previous pump, run 1 | 585 | 584 | 1 | 0.787 ms | 16.535 ms |
| Previous pump, run 2 | 577 | 576 | 1 | 0.596 ms | 16.535 ms |
| Updated pump, run 1 | 586 | 23 | 563 | 1.189 ms | 16.535 ms |
| Updated pump, run 2 | 586 | 36 | 550 | 1.202 ms | 16.535 ms |

All four clients connected, closed their browser, exited with code zero, and
left no game or CEF helper process. A separate `-nosound -gl` joined-client
run also closed cleanly and found only 35 of 586 callbacks in flight. The
higher graphics work is consistent with transferring and uploading more of
the page's animated images; the 60 FPS game cap still held in these runs.
This trace measures paint availability and frame work, not the number of
frames shown by the display or uncapped game FPS.

In two further Metal runs, the CEF host was stopped with `SIGSTOP` six seconds
after client launch while the animated MOTD was active. `quit` exited zero in
28.672 and 28.752 seconds, and neither run left a game or CEF helper process.
The stopped hosts could not flush a frame trace; these runs check bounded
teardown rather than paint cadence.

With the loopback server stopped, both app copies replayed the same 7,102-frame
`CSGO_MULTIHEAD1.dem` at titled 1280×720, uncapped Metal, `-nosound`. The
[fixed-demo analyzer](metal-benchmark.md) reported 159.79 FPS and 57.01 FPS
1% low for the previous pump, versus 161.38 FPS and 57.18 FPS 1% low for the
updated pump. Both replays completed and exited cleanly. A single pair does
not establish a gameplay FPS gain; it shows no obvious fixed-demo regression
from the browser pump change. Raw frame traces and the comparison JSON are in
the ignored `build/metal-cef-20261120/browser-pump-gameplay-check/` directory.

A native browser controller fixture now sends two 1024×768 full backing
images, with the second gated on the first acknowledgement, and requires both
to arrive within ten simulated 60 Hz game frames. The browser suite passed
6/6 in macOS Release and sanitizer builds; the controller fixture passed on
Windows x64 with MSVC `/W4 /WX`.

## Larger-window live check

Four more joined-client runs used the same animated page and server with a
2560×1440 borderless Metal window, `-nosound`, and the same 60 FPS cap. The
MOTD browser image reached 728×432 pixels, so this checks larger live paints
than the titled 1280×720 runs; it is not a full-window browser test. Both app
copies used the same assets and host binary. The runs alternated updated,
previous, updated, previous.

| Pump | CEF paint callbacks | Callback rate | Callbacks with paint in flight | Median graphics work | Game interval p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Updated, run 1 | 585 | 59.98 Hz | 78 | 1.349 ms | 16.535 ms |
| Previous, run 1 | 395 | 40.37 Hz | 394 | 0.697 ms | 16.535 ms |
| Updated, run 2 | 584 | 59.83 Hz | 40 | 1.340 ms | 16.535 ms |
| Previous, run 2 | 372 | 38.01 Hz | 371 | 0.611 ms | 16.535 ms |

Each run connected, closed the browser, exited zero, and left no game or CEF
helper process. Only one connected game frame per run exceeded 17 ms. The
updated pump does more graphics work because it transfers more live browser
images; these capped runs did not show a frame-interval regression. This
measurement remains limited to browser callback availability and game CPU
timing, not displayed frame cadence. The raw traces and metadata are in the
local ignored `build/metal-cef-20261120/browser-pump-hires/` directory.
