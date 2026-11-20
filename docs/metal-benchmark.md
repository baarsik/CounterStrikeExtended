# macOS Metal fixed-demo benchmark

Use an identical Counter-Strike demo and game assets in two **isolated, signed**
macOS app copies. Overlay each app with the complete Release runtime from its
respective source revision. Record the source revision, runtime hashes, demo
SHA-256, macOS version, display resolution and refresh rate with the results.
Keep the same visible window mode and display for every run.

Put the demo in each copy's `Contents/Resources/runtime/cstrike` directory and
use a matching `config.cfg`. A benchmark command file in that directory should
set the measured graphics settings and finish with `gg <demo-name-without-.dem>`.
For example, the 60,019-frame `codex_longest_20261117.dem` fixture has SHA-256
`b50b9ed620032eb1a08014bbae0929f258d939bfd48d23fc34f85b8853f3298a`.
Its command file uses `fps_override 1`, `fps_max 9999`, `gl_vsync 0`,
`gl_ansio 16`, `gl_texturemode GL_LINEAR_MIPMAP_LINEAR`, `gl_use_shaders 1`,
`cl_shadows 1`, and `r_detailtextures 0` before `gg`.

For a borderless 2560×1440 run, launch each isolated app with the same arguments:

```sh
CSE_FRAME_TIME_LOG=/absolute/path/to/run/frames.csv \
  /absolute/path/to/Counter-Strike.app/Contents/MacOS/CSEGameEntry \
  -windowed -noborder -forceres -w 2560 -h 1440 -novid -nosound -metal \
  -port 27049 +exec cse_benchmark.cfg \
  > /absolute/path/to/run/game.log 2>&1
```

Run one warmup per build, then alternate baseline and candidate runs. Keep the
game focused and avoid building or running other GPU workloads during the series.
Record the process exit code. A completed `gg` replay issues the engine's `quit`
command; verify that the game, browser host and its helpers have exited. The
frame log is written during normal shutdown. Use separate result directories
for every run. `CSE_METAL_FRAME_STATS` enables extra per-frame Metal counters
for diagnosis, but leave it unset for comparative FPS runs.

Compare a matched pair with:

```sh
python3 tools/analyze_metal_benchmark.py \
  /absolute/path/to/baseline-run /absolute/path/to/candidate-run \
  --demo codex_longest_20261117.dem --frames 60019 \
  --output /absolute/path/to/comparison.json
```

The analyzer requires a complete replay and at least 99% connected frame
coverage. It excludes the first 500 connected samples, rejects focus or menu
changes in the measured window, and reports average FPS, 1% low FPS, and
median/p95/p99 frame intervals. Report individual runs as well as paired
changes; a single favorable run is not enough to establish an improvement.
