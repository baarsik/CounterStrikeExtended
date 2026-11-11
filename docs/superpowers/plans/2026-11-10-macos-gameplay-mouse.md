# macOS Gameplay Mouse Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the system pointer out of active gameplay and preserve smooth camera motion during fast mouse movement in native Half-Life and Counter-Strike.

**Architecture:** Repair the existing native mouse owner and actual input consumers after proving which boundary loses motion or capture. Menu, focus and window lifecycle transitions retain explicit ownership. Do not conceal the problem by changing saved input settings.

**Tech Stack:** C++17, AppleClang, macOS 14+ ARM64, pinned source-built SDL3 3.4.20, existing GoldSrc hardware/software and game clients.

**Spec:** The user's active-gameplay reports in this chat and `docs/native-migration.md`. They explicitly clarified fast movement causes camera jumps or pauses, not horizontal picture splits. This is a bounded repair within the already authorized complete migration; existing migration constraints and authorization apply.

## Global Constraints

- Preserve maintained Win32 and Windows x64 behavior, interface slots, protocol48 and retained per-file policies.
- macOS 14+ ARM64 is native; use pinned SDL3 3.4.20 and source-owned providers. Do not import external code.
- Keep mouse sensitivity, filtering and raw-input choices meaningful. Do not force `m_rawinput`, `cl_mousegrab`, VSync or frame caps as the product fix.
- No global mouse/accessibility/preferences changes, extra permissions, event taps, private layout/RVA assumptions, blanket cursor hiding outside the owned focused game or repeated per-frame warps as a new capture mechanism.
- Preserve menus, console, MOTD interaction, native text input and previously fixed press-and-hold and cursor-location-assistance behavior.
- Keep changed C++ scoped and formatted under root AGENTS.md. Retained manifests own generated inventory; regenerate when needed.
- Original Steam assets are read-only. Controller owns live user apps, shared integration and VM1; implementer uses isolated source/builds only and never operates VM2.
- Actual-source RED/GREEN, actual owning image builds and real native gameplay are distinct evidence. Simulated coordinates alone do not establish physical smoothness or pointer disappearance.

## Review Focus

1. Fast movement crossing the legacy recenter threshold or the window edge preserves all accepted motion exactly once.
2. Focus loss, menus, console and MOTD release capture; returning to gameplay reacquires it without a stale delta or visible-pointer interval.
3. Both raw-input choices and both mouse-grab choices work in CS and Half-Life, including windowed and fullscreen presentation.
4. Reentrant SDL pumping or a newer UI cursor request prevents an older transaction from reclaiming capture or resetting fresh motion.
5. Resize, minimize/restore and restart invalidate old native bindings and keep logical/pixel coordinates distinct.

### Task 1: Diagnose and repair actual gameplay mouse capture and delta loss

**Files:** Existing `GoldSrc-SDK/engine/native_mouse_window.{h,cpp}`, `native_mouse_cocoa.cpp`, `sys_sdlwind.cpp`, `vgui2/BaseUISurface.cpp`, `VGUI_EngineSurfaceWrap.cpp`, `GoldSrc-SDK/common/sdl3_{input,policy,native_cocoa_mouse}*`, game `inputw32.cpp` only where the demonstrated cause requires it. Extend `tests/native/native_mouse_window/` and the existing SDL/provider tests; create a focused actual-source event-loop fixture if needed. Avoid broad input rewrites.

**Interfaces:** Preserve public engine/game mouse ABI, the fixed-width V1 SDL mouse contracts and their existing caller guards. Consume the actual `NativeMouseWindow` owner, existing SDL mode bridge and actual game `IN_Accumulate`/`IN_MouseMove` bodies. Any justified native policy change must update all affected callers and mode predicates together and document how raw-off semantics remain meaningful.

- [ ] Step 1: Inspect current capture and complete delta flow from Cocoa through SDL to both actual client consumers. Save a concise diagnosis and proposed minimal repair before editing production. Prove the cause with an actual-source failing test, distinguishing accepted physical deltas from warp-generated deltas and discarded post-pump reads. If capture semantics require an architectural change, ask the controller with evidence before implementation.
- [ ] Step 2: Add behavioral regressions for a fast burst crossing a recenter boundary, motion arriving during a warp/pump, captured-to-menu-to-captured transitions, focus loss/return, both raw/grab settings, and stale transaction cancellation. Execute actual production bodies; use doubles only at explicitly named OS boundaries. Retain a real behavioral RED with command/output, not source-text assertions.
- [ ] Step 3: Implement the smallest cause-specific repair. Keep one native capture owner; consume accepted camera motion exactly once, preserve fractional remainders and acceleration/sensitivity choices, and restore menu/focus cursor behavior. Do not promote a change based on an inactive-window snapshot.
- [ ] Step 4: Run the covering component suites in Debug and Release and sanitizer cases where relevant. Build actual affected native owners under retained policies. Preserve immutable source/image manifests and a coherent frozen candidate for controller gameplay tests. Run Windows owning compilation if changed common code can affect it; controller coordinates VM1 separately.
- [ ] Step 5: Self-review and commit only task changes, excluding the explicit constructor-only prerequisite and all earlier migration work. Write the full report with diagnosis, RED/GREEN commands/output, source/image hashes, changed paths, limitations and exact controller launch instructions. Do not claim full native gameplay acceptance or whole migration completion.

Controller acceptance after source review: actual Half-Life and Counter-Strike rapid turning, menu/console/MOTD, focus return, minimize/restore and windowed/fullscreen transitions; user-observed smoothness is required alongside machine state. Keep broader server-browser, video-mode, audio-focus and lifecycle findings in their own queues.
