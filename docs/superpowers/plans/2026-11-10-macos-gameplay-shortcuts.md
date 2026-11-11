# macOS Gameplay Shortcuts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reserve Control-Space for ordinary duck/jump gameplay while preserving text entry and system shortcuts.

**Architecture:** Add a guarded app-local NSEvent policy to the owned SDL Cocoa provider, after SDL's existing key delivery. Enable it from a versioned engine-private current-window owner only for active gameplay keyboard/UI intent, with explicit early hook removal.

**Tech Stack:** Pinned SDL3.4.20, existing Objective-C++/ARC native provider, C++ engine HW/SW and VGUI1/VGUI2, macOS14+ ARM64, retained Windows compilation policies.

**Spec:** `docs/superpowers/specs/2026-11-10-macos-gameplay-shortcuts-design.md`. Fresh architecture builder check returned nothing important left; report is `build/macos-runtime-bringup-20261109/user-runtime-findings-20261110/gameplay-shortcuts-builder-questions.md`.

## Global Constraints

- Preserve upstream SDL archive/source/dynapi bytes, all old provider API bodies/results/service readiness, public engine/client ABI, saved fields, protocols and assets.
- No global preference, event tap/global monitor, hotkey registration, secure-input mode or permission grant. Do not fix the explicitly deferred microphone startup-consent issue.
- Do not change key bindings, SDL key delivery, raw acceleration/sensitivity/filtering, cursor mathematics, command buffer/wait/bhop semantics, FPS/VSync, audio/career activation or renderer selection. No synthesized/duplicated key events.
- Current accepted CS110 files and live HL109 files remain untouched during source work. Controller alone integrates root and stages live apps/VM1. Never VM2.
- One source implementer at a time, existing clean linked macos-dedicated worktree at17b99c9549bd50793093804677488a98d792345a, at most two compiler processes, retained owning build flags, immutable source/image/evidence packets. No unrelated snapshot imports.
- User has delegated necessary implementation choices and is no longer answering. Continue autonomously; native UI automation does not constitute human physical gameplay/performance acceptance.

## Review Focus

1. A filtered Control-Space/Control-Shift-Space must preserve exactly the existing SDL down/up stream while preventing the app's layout-switch action.
2. Chat, console, menus, custom cursors, inactive/foreign windows and Command/Option combinations must retain their normal behavior; hiding the mouse alone is not gameplay keyboard intent.
3. Monitor installation/removal failures, reentrancy, in-flight callbacks, window identity reuse and teardown must not leave a callable block in an unloaded image or authorize stale ownership.
4. Latest pending UI intent must disable the policy before subsequent native dispatch, and a failed/stale transaction must not silently restore gameplay policy.
5. Ordinary steady gameplay must not repeatedly install/remove hooks, query expensive native hit tests, pump/drain input, or change accepted camera/scrollwheel behavior.

### Task 1: Add the unused guarded native shortcut policy

**Files:** Create `GoldSrc-SDK/common/sdl3_native_cocoa_keyboard.h` and a narrowly scoped private header if required. Modify only the owned `sdl3_native_cocoa_mouse.mm` for the new service/state and `GoldSrc-SDK/external/SDL3-source/UPSTREAM.md` for its contract. Add focused actual-provider fixtures under `tests/native/gameplay_shortcuts/`. Do not modify old mouse bodies/service completeness, engine callers or upstream SDL.

**Interfaces:** Add `uint32_t CseSdl_SetCocoaGameplayShortcutsV1(uint32_t version, void *nativeWindow, uint32_t enabled)`, pure C keyboard contract version1, enabled0/1, existing CSE Cocoa result values with Ready1 as success. Arguments are borrowed per call; no retained window/engine callback. New provider-owned monitor-token state has explicit removal/lifetime semantics.

- [ ] Step 1: Inspect actual provider guards/lifetime and pinned SDL Cocoa dispatch. Propose exact minimal private typed services/state/event-filter signatures and teardown behavior to the controller before production editing; settle in-flight callback and reentrant invalidation admission explicitly. Preserve baseline actual API-absence RED if used and distinguish it from the already recorded real-provider Control-Space behavior RED; do not present a missing symbol as a gameplay regression test.
- [ ] Step 2: Add actual-source behavioral cases for guard order/version/null/enabled bounds, absolute/relative mode, owned/foreign/inactive/key-window contexts, install failure, idempotent enable/disable, conflicting/replaced/reused window identity, callback reentrancy and retirement while callback is live. Event cases cover Space with either/both Control and optional Shift; Command/Option variants, other keys, key-up and foreign contexts pass unchanged. Complete new bodies run with named SDL/AppKit seams; no duplicate implementation model or fake claim of physical input.
- [ ] Step 3: Implement the controller-approved provider state and app-local key-down filter, preserving old entrypoints byte-for-byte. Callback uses no engine pointer and performs no key injection, layout query, log, mouse mutation, input pump/drain or expensive mouse hit test. Remove the monitor before retirement; failure must remain visible/retryable and must not allow image unload with an in-flight callback.
- [ ] Step 4: Run focused Release/Debug/ASan+UBSan cases and relevant unchanged-provider regression controls. Compile the real owning Objective-C++ TU and full-link matching Release/Debug SDL images with retained policy and pinned unchanged upstream/dependency/object inputs. Provide a video-only real-provider probe using the new export and unchanged SDL key stream for controller native OS qualification; no audio, permissions or game app changes.
- [ ] Step 5: Self-review, freeze exact source/commands/output/dependency/image hashes, commit task-only paths and yield. Controller independently reviews and tests the actual exported policy against native layout switching before Task2 enables it. Report limitations and exact old-body/ABI proof; no client or migration completion claim.

### Task 2: Bind the policy to current gameplay keyboard ownership

**Files:** Existing engine-private `native_mouse_window.{h,cpp}`, `native_mouse_cocoa.cpp`, both actual event-loop owners in `sys_sdlwind.cpp`, and minimal actual UI/cursor lifecycle call sites only if inspection proves necessary. Extend existing native owner/caller/lifecycle fixtures and `tests/native/gameplay_shortcuts/`; do not add a new public UI mode or change client mathematics.

**Interfaces:** Consume Task1's exact export. The builder proposes a minimal optional platform callback and private owner methods/state, including gameplay keyboard intent, current/pending cursor intent and generation; controller rules on exact signatures before editing. No native callback into engine code.

- [ ] Step 1: Read complete current owner, both HW/SW pumps, cursor callers, key_dest/con_forcedup and real GameUI/console/chat context. Propose the exact private boundary, eligibility predicate, before-pump update order, pending UI precedence, retry behavior and explicit disable-before-destroy/unload order. Controller resolves before code changes.
- [ ] Step 2: Preserve actual-source RED for missing gameplay policy through the real decision/caller paths. Add cases for gameplay versus chat/console/menu/custom cursor, either VGUI owner, both loops, active/key/focus/window/fullscreen changes, pending/failed UI requests, native setter failure, nested invalidation, in-flight callback retirement and window replacement. Positive controls pin unchanged key delivery, steady-state setter count and existing mouse cutoff/first-delta/wait behavior.
- [ ] Step 3: Integrate the policy only for current committed gameplay intent and active gameplay keyboard destination, refresh before native pumping, and honor latest UI intent. Failure/teardown must preserve a safe retryable binding and propagate before window/SDK retirement. Keep AppActivate audio/career and all mouse formulas/order unchanged. Future screenshot suspension must be able to disable effective keyboard policy without losing logical intent.
- [ ] Step 4: Run covering Release/Debug/sanitizer checks, compile actual affected native owners/consumers, full-link coherent HW/SW and genuine-main launcher tuples with the Task1 provider, and pin all changed/unchanged source/object/dependency/image inputs. Give the controller precise retained Windows owning compilation or nonselection/preprocess proof needs. No unnecessary broad rebuilds or warning weakening.
- [ ] Step 5: Self-review, freeze, commit only task changes and yield for independent task review. Controller stages immutable candidates and validates native OS layout preservation plus actual SDL duck/jump events, ordinary keys, chat/console/menu layouts, focus/fullscreen/close/restart, Cmd-Shift-4 and unchanged audio/hardware renderer. Human fast-turn/bhop acceptance remains unverified while the user is away. Final broad branch review occurs at parent migration completion.
