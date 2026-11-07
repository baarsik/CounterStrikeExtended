# Task 5 — qualification tooling and evidence handoff

Base/product source: `3d62f6a`. Implementation scope is tools/tests/documentation only;
no production SDK, CMake, ABI, renderer/audio backend, warning/assertion/FP policy or
provider allowlist changed. Root alone owns all VM/Parallels/build/game/provider
execution, final measurements and independent whole-branch review.

## Delivered

- `tools/qualification/invoke_paired_benchmark.ps1`: VM-ready bounded interactive
  paired fixed-demo runner. Consumes two separately staged/qualified owned
  sandboxes, one shared ASCII settings file and the game-specific accepted demo SHA.
  Pins recording identity; compares native non-migration provider paths/hashes and
  fixed map/PAK/WAD inputs; records before/after native manifests outside process
  lifetime. Resets config/autoexec before every run, archives identical settings,
  forces viewport/vsync/player settings, excludes one warmup per version and
  alternates measured AB/BA pairs (default seven). Unique prefix reservations,
  bounded waits, launched-PID-only cleanup, full EOF/CLI/profile coverage gates and
  visible/foreground PID observations preserve unsuccessful attempts as evidence.
  No module hashing, console polling, PresentMon or screenshot IO during replay.
- `tools/qualification/analyze_benchmark.py`: raw-evidence revalidation and paired
  descriptive analysis. Rechecks exact demo/settings identity, complete pair/order
  coverage, warmups, window/PID/exit/cleanup gates, raw logs, report timing and host
  profile counts. Keeps zero FPS as unbounded slow frames and 999 as a possible
  fast sentinel. Reports nearest-rank p50/p95/p99 lower/upper timing bounds and
  explicitly labeled midpoint estimates, per-run maximum/counts, separate rounded
  CLI aggregate timings and median paired percentage differences. Refuses to
  overwrite an existing analysis artifact.
- Twelve analyzer tests cover quantization/sentinel/zero, non-newline console
  prefixes, profile/CLI coverage, incomplete EOF, duplicate reports/markers,
  paired ratios, full series/warmup/order, tampered settings/raw summaries and
  missing/mismatched/hidden/background foreground evidence.
- Qualification README documents runnable VM parameters, staging parity, smoke vs
  full series, settings replacement, precision and overhead/VM limitations.
- `docs/sdl3-migration-validation.md` and `docs/sdl3-final-evidence.json` preserve
  exact product archive provenance and Task 4 closure, plus an explicit pending
  final acceptance matrix. They are deliberately an in-progress handoff, not an
  assertion that root-owned remaining cases or all behavior passed.

## Verification performed on authoring host

- Test-first red: analyzer boundary tests failed with ModuleNotFoundError before
  implementation; then passed with the implementation.
- `python3 -m unittest discover -s tools/qualification/tests -v`: **50/50 pass**.
- `python3 -m unittest discover -s tools/tests -v`: **11/11 pass**.
- `git diff --check`: pass.
- Parser exercised against actual saved SDL2 experimental profile logs: CS 667
  CLI / 668 profiles; HL 655 CLI / 656 profiles. Both parsed full EOF and retained
  all pre-EOF profiles. This is parser corroboration only, not final paired evidence.
- No host `pwsh` is installed. PowerShell parser validation, driver smoke and live
  foreground behavior remain root-owned VM checks. No VM or game was run here.

## Remaining root gates

1. Parse/smoke the final driver from the current source. Existing baseline
   sandboxes contain experimental configs/raw-overlay natives; stage new matching
   approved providers/CRTs and assets before using the strict parity checks. Use a
   fresh evidence prefix for each retry. Shared settings are deliberately written
   only to the explicitly supplied owned sandboxes.
2. Finish four fresh-source preset configure/build pairs and actual optional ON
   qualification. Keep source archive `8fc88a4950e5eb835b6c44c2e8eb33eecd20cc474e7fac0d9791b28c7ed13559`
   distinct from subsequent tool/document commits; compare product subtree IDs.
3. Run fresh final Debug/Release CS/HL sign-on/alive movement/full EOF/clean shutdown;
   static repository, actual staged sandbox and actual live loaded-module closure;
   final renderer screenshots/audio output and dedicated/video cases.
4. Exercise feasible fullscreen/fallback/focus/capture/UI text/clipboard/device
   cases. Prior CUA/native-input delivery failures are unverified harness results,
   not manual acceptance or demonstrated app defects. Mark unavailable native
   hardware/gamepad/DPI/refresh/gamma cases explicitly unverified.
5. Run one excluded warmup/version + seven AB/BA measured pairs per game while VM
   is otherwise idle. Analyze complete series and retain raw logs/manifests. Do
   not claim uninstrumented/native-hardware pacing from instrumented VM results.
6. Update the in-progress docs/evidence manifest using actual root outputs and
   obtain independent whole-branch review/resolution before final closure.

## Commits

Implementation commit recorded in the parent handoff after creation; this report
is part of that same reviewable tools/docs commit.

## Root VM smoke and implementation self-review

Implementation commit: `23f7e1d`. Root subsequently supplied actual VM smoke evidence:
`E/benchmark-smoke/cstrike-harness1-series.json` and
`E/benchmark-smoke-analysis.json`. Root reported PowerShell AST validation with
zero errors; the authoring-host agent did not run the VM.

Self-review read all four raw run manifests/logs, the archived settings, series
provider/asset manifests and saved analysis, then reran `analyze_series` read-only
on the actual files. The regenerated object exactly equals the saved analysis.
The run sequence is baseline/candidate warmups, then candidate/baseline measured
pair; all four report success, exit zero, 666 CLI frames / 667 host profiles,
complete ordered EOF/final markers and no forced cleanup. Arguments are identical.
Both native sets have 43 entries; both matching map/PAK/WAD manifests have 32.
The same archived settings hash is used throughout; no unknown-command or missing
settings-exec error was found in the console logs.

Each process has 124–143 observations, acquires its visible foreground window
within 332–638 ms, retains owned foreground afterward and has exactly one final
invisible observation during clean window teardown. These last observations are
within the documented 250 ms process-shutdown tolerance; no mid-run background or
hidden interval was found. Three profiles contain one zero-FPS sample each; the
analyzer retains them and correctly reports an unbounded maximum upper duration.
The fourth has none. No driver/analyzer inconsistency or additional material
qualification-tool finding was established by this review.

This is **one-game, one-pair harness smoke only** at product `3d62f6a`. Its single
paired difference is not final performance acceptance. Final CS/HL seven-pair
series remain held for the accepted production cursor-ownership P2 fix from
`whole-branch-review.md`, then qualification against the new product identity.
Root also reports all four fresh-source presets, explicit optional ON, 43-PE
fixture and final Release/Debug CS/HL probes/sandbox audits passed at `3d62f6a`;
those facts await the final consolidated evidence update after the cursor fix.
The implementation agent now waits for root's final numbers/matrix evidence.
