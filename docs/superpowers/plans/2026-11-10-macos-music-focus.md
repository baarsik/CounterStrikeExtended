# macOS Music Focus Resume Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resume music paused by loss of game focus when native macOS gameplay regains focus.

**Architecture:** Repair the existing application-activation/CD-controller boundary. Ordinary queued pause/resume and explicit MP3 stream suspension are distinct states; retain the existing worker and restore only legitimate playback. Preserve the current input and native ownership fixes.

**Tech Stack:** C++17, AppleClang, macOS 14+ ARM64, existing CD command worker, mpg123 1.33.7 and SDL3 3.4.20.

**Spec:** The user's complete native migration request and `docs/native-migration.md`. The controller's actual-source RED is `build/macos-runtime-bringup-20261109/audio-focus-source-probe/receipt.json`: both actual `CGame::AppActivate` bodies pause ordinary MP3 playback on focus loss but fail to resume on gain; explicit `Resume()` works. This does not establish the cause of earlier device stalls or the HEV music timing.

## Global Constraints

- The user explicitly deferred microphone authorization/startup work. Do not edit recording/voice initialization, request/grant/reset OS permissions, or change application permission descriptions.
- Preserve Windows x86/x64 behavior and original owning compile policies; keep public interfaces/vtables, protocol and save formats unchanged.
- Do not change input settings, bindings, cursor policy, mouse accumulation, frame caps, VSync, providers, system audio routing or global device settings.
- Preserve track, loop, position, stop/pause intent, failed-resume recovery and audio module shutdown ownership. No per-frame playback restart, new worker or broad audio redesign.
- All original Steam assets are read-only. Controller owns live apps and VM1; implementer uses the existing isolated source worktree and immutable build artifacts, with parallelism limited to two.
- Apply root C++ conventions and keep edits narrow. No production edit in the controller. Separate actual-source tests, owning builds and audible game acceptance.

## Review Focus

1. A normal playing MP3 survives focus loss/gain without starting at the beginning.
2. Stopped, disabled and legitimately nonplaying controllers do not acquire unwanted playback.
3. Repeated/rapid focus changes preserve command ordering and do not strand a queued pause.
4. Explicit stream suspension and failed device restoration retain retry state instead of falsely reporting playback.
5. Shutdown rejects/drains focus work before engine unload; no callback can outlive its owner.

### Task 1: Repair the actual music focus transition

**Files:** Modify the two existing `CGame::AppActivate(bool)` implementations in `GoldSrc-SDK/engine/sys_sdlwind.cpp`, and existing controller methods in `GoldSrc-SDK/engine/cd.cpp`/`cd.h` only when required. Add a focused `tests/native/music_focus/` fixture; reuse existing worker/lifecycle test infrastructure where applicable. Do not edit `voice_record_sdl.cpp` or other microphone code.

**Interfaces:** Consume existing `ICDAudio::Pause()`, `Resume()`, `MP3_Resume_Audio()` and controller worker callbacks. Preserve their public signatures and vtable layout; avoid introducing an interface solely for the test. Any required private engine helper must remain internal and have a justified concrete signature in the implementation report before dependent code uses it.

- [ ] Step 1: Reproduce the controller's RED with actual source/owning declarations, then inspect ordinary pause versus explicit stream suspension and queue order. Write the minimal cause-specific approach before editing production; request a controller ruling if a new cross-owner boundary is necessary.
- [ ] Step 2: Add behavioral cases for all five Review Focus items. Use actual activation/controller bodies and named backend/OS seams; do not replace the decision logic with a test model. Run the unchanged source and preserve its failing command/output.
- [ ] Step 3: Repair focus regain through the existing lifetime-safe controller path, preserving suspension restoration and failure/stop semantics. Keep Windows behavior and current mouse functions unchanged; no hardware workaround.
- [ ] Step 4: Run covering Debug, Release and relevant sanitizer/component cases with exact commands/output. Build affected HW/SWDS and matching launchers under retained owning policies. Regenerate cohort hashes from the actual new images; preserve the reviewed genuine-main checks and current mouse objects. Keep all prior frozen packets immutable. Report Windows compilation needs to the controller rather than operating VM1.
- [ ] Step 5: Self-review, commit only this task's source/tests, and write a report containing diagnosis, RED/GREEN, changed paths, source/image hashes and exact staging instructions. Controller independently reviews before integration and actual audible/focus tests. Do not claim full migration completion.

The current Counter-Strike fixture may remain open for the user; do not operate or update it. Existing authorization is to continue the migration autonomously using the established implementation/review workflow; this scoped bug repair does not require a new plan approval pause.
