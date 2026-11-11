# macOS System Screenshot Capture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow macOS region screenshot selection during gameplay and restore normal camera capture afterward.

**Architecture:** Add a synchronous, mode-independent Cocoa interaction-owner query to the owned SDL provider. Use it from an engine-private coordinator that separates current UI intent from effective capture, fences foreign input and preserves the existing generation and motion-cutoff rules. Actual app activation and audio behavior remain independent.

**Tech Stack:** Existing C++/Objective-C++ ARM64 engine and owned SDL3 3.4.20 provider; public AppKit hit testing; retained Windows x86/x64 policies and actual-source native fixtures.

**Spec:** `docs/superpowers/specs/2026-11-10-macos-system-capture-design.md`. The required fresh builder check found nothing important left; its report is `build/macos-runtime-bringup-20261109/user-runtime-findings-20261110/capture-design-builder-questions.md`.

## Global Constraints

- No event tap, Input Monitoring/Screen Recording permission, global preference, system audio route, microphone initialization or recording-permission change. The microphone startup issue is explicitly deferred.
- Preserve pinned upstream SDL archive/header/dynapi bytes. New exports belong to the existing owned CSE Cocoa provider; preserve all old result values and calling conventions.
- Preserve current mouse mathematics, raw acceleration/sensitivity/filtering, bindings, supported scrollwheel `+jump; wait` behavior, FPS/VSync and audio/career activation semantics. Do not flush or rewrite the command buffer.
- Preserve current-window/version/reentrancy checks. No post-reacquisition setter drain/pump may discard new motion. No repeated setters, drains or warps in steady gameplay or while an overlay remains active.
- Bound expensive native hit testing to one query per 10 ms in the steady outer poll loop; no sleeps, new threads or retained native pointer/callback owner.
- One source implementer at a time. Controller owns root integration, live apps and VM1. Use the existing isolated clean worktree, immutable source/build packets and at most two concurrent compile commands.
- The user-accepted CS110 app remains untouched during implementation. Its earlier engine tuple plus corrected timer client runs fullscreen on M5 Max with normal RODECaster sound. The later music-engine tuple's observed CPU-renderer fallback remains an independent runtime gate; production Metal is still unselected.
- No public engine/client ABI, layout, protocol or save-format change. Apply root C++ style narrowly. Do not import unrelated snapshot commits or yielded dedicated-lifetime work.

## Review Focus

1. A native overlay can own mouse interaction while app/key flags remain true; release and restoration must not depend solely on focus events or the intercepted shortcut.
2. Menu/custom-cursor intent can change during selection; cancellation or completion must honor the newest live owner and must not recapture a menu, foreign focus or retired window.
3. Foreign mouse motion/clicks and held input cannot become camera motion or game actions after restoration; the first newly accepted gameplay delta must survive.
4. Setter/query failure, nested invalidation, mode changes and teardown must leave retryable state and never commit against a replaced window or unload an owner with pending callbacks.
5. High-frequency outer polling and legitimate wheel/wait sequences must retain normal input throughput without unbounded WindowServer queries, repeated mode work or cursor visibility regressions.

### Task 1: Add and qualify the public native interaction-owner query

**Files:** Modify `GoldSrc-SDK/common/sdl3_native_cocoa_mouse.h`, `sdl3_native_cocoa_mouse_private.h` and `sdl3_native_cocoa_mouse.mm`. Extend focused actual-provider tests in `tests/native/native_mouse_window/` or a narrowly scoped `tests/native/system_capture/` fixture. Update the provider's retained `UPSTREAM.md` contract description only as needed; no upstream source or generated target change is expected.

**Interfaces:** Produce additive `uint32_t CseSdl_CheckCocoaInteractionOwnerV1(uint32_t version, void *nativeWindow)`. Preserve all existing result values; add `CseSdlCocoaMouseNotInteractionOwner = 8` for a validated owned window whose interaction is foreign/inactive. Existing Ready remains 1. Other guard failures keep their existing error results. The corresponding private typed service entrypoint borrows service/window references only for the call.

- [ ] Step 1: Inspect the actual provider guards and default service bindings. Reproduce the missing boundary with complete owning source and typed native seams: active/key true plus foreign mouse hit-test owner must yield NotInteractionOwner in absolute and relative modes. Preserve original-source RED without substituting a detector model.
- [ ] Step 2: Add cause-specific cases for own/foreign/transparent hit-test results, inactive/key/minimized/invalid geometry, all existing guards, readiness in both modes and no side effects/retained pointers. Preserve existing sampler and cursor-policy behavior; do not make old entrypoints require the newly added service callbacks.
- [ ] Step 3: Implement the proposed export/private body with existing guard order and public AppKit bindings. Document exact private seam signatures before use. No input pump, consumption, warp, presentation mutation or mode setter is allowed inside the query.
- [ ] Step 4: Run covering Release/Debug/sanitizer actual-source cases. Compile the owned provider TU and full-link matching native SDL libraries under retained policies, proving upstream bytes and all unchanged input objects stable. Provide a minimal video-only real-provider probe that logs ownership at bounded cadence for controller Cmd+Shift+4 testing; no audio/capture permission or game app modification.
- [ ] Step 5: Freeze source/images/probe and exact commands/output/pins, self-review and commit only task paths. Report to the controller. Independent review and the controller's actual overlay signal qualification precede Task 2's use of this export.

### Task 2: Coordinate effective capture across system overlay transitions

**Files:** Modify the existing `GoldSrc-SDK/engine/native_mouse_window.{h,cpp}`, `native_mouse_cocoa.cpp`, both relevant bodies/call sites in `sys_sdlwind.cpp`, `vgui2/BaseUISurface.cpp` and `VGUI_EngineSurfaceWrap.cpp` as needed. Use `sys_sdlwind.h` only if required for private coordinator state. Add focused covering actual-source tests under `tests/native/system_capture/` and extend existing mouse ownership fixtures where they already own a boundary.

**Interfaces:** Consume Task 1's exact query/results through a borrowed engine-private platform service. Reuse `NativeMouseWindow`, `NativeCursorTransaction`, `ClearIOStates()`, `ClientDLL_ActivateMouse()` and `ClientDLL_DeactivateMouse()`. New engine-private decision/apply types or signatures must be proposed concretely with call order and lifetime in the report, and receive a controller ruling before dependent implementation uses them. Do not add a client or public interface just for this task.

- [ ] Step 1: Read both complete event loops and cursor callers plus actual input reset/activation paths. Propose the minimal coordinator state/signatures and order for release, event fencing and reacquisition. Include how latest pending UI intent, grab, query errors and nested invalidation are handled; controller resolves the concrete boundary before code changes.
- [ ] Step 2: Include initial active/key Bind and ordinary visible/custom UI keyboard/text navigation with the pointer outside content, distinct from an established gameplay suspension; retain real overlay-to-menu/error/dirty-shortcut controls. Preserve an actual-source RED for active/key true plus foreign overlay: camera capture currently stays relative/centers the pointer. Add behavioral cases for all five Review Focus items, including both engine owners, both VGUI owners and raw/grab combinations. Use real decision bodies with named SDL/OS seams, not a duplicate state-machine model.
- [ ] Step 3: Implement the approved engine-private coordinator. Keep logical native app activation and audio/career behavior unchanged. Suspend client camera consumption, release effective relative/grab/presentation capture once, gate the legacy absolute warp path and foreign input delivery, and continue lifecycle/quit processing. Reacquire from current UI intent only; reset/drain old ownership before the first setter and retain fresh post-setter motion. Define and test the queued-event cutoff explicitly.
- [ ] Step 4: Run covering Debug/Release/sanitizer suites and cadence/steady-state controls. Rebuild actual affected native engine/consumer TUs, full-link HW/SWDS and matching launcher cohort with the Task 1 provider under retained policies. Pin source/object/dependency closure, genuine-main bootstrap objects, symbols and all new image hashes. Report precise Windows owning compilation needs to the controller; no broad unnecessary rebuild or flag suppression.
- [ ] Step 5: Self-review, freeze source/coherent images, commit only task changes and report exact commands/output/pins. Controller independently reviews before staging. The final runtime gate covers real Cmd+Shift+4 drag/cancel/complete, subsequent fast turns, wheel bhops, menu/focus/fullscreen transitions, no pointer and normal audio in both games. Renderer identity must be hardware for a hardware test; no source-only or synthetic-input result closes that gate.

The two tasks are separate reviewable deliverables: an unused additive query can be rejected independently of the engine policy. No incomplete capture behavior is staged between them. The user has already authorized necessary migration fixes; the public-API, app-local repair introduces no new approval pause.
