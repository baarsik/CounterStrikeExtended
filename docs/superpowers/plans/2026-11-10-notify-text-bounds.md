# Console Notify Bounds Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove garbage suffixes and the high-resolution notify-buffer overread.

**Architecture:** Bound the existing actual drawing loop by console columns, container capacity and first NUL. Preserve message production and all other drawing paths.

**Tech Stack:** Existing C++ engine HW/SW, native ARM64 and retained Windows x86/x64 owners, actual-source fixtures and sanitizers.

**Spec:** `docs/superpowers/specs/2026-11-10-notify-text-bounds-design.md`.

## Global Constraints

- No public ABI/layout/save/protocol, client/asset, developer/FPS setting, input, audio or renderer-selection change.
- Preserve every preceding character, font/color, position/width result, notification timing/line bookkeeping and independent chat/debug drawing.
- One source implementer, global at most two compilers, root/live/VM1 controller-only, original Steam assets read-only and no VM2 or global permission/route changes.
- The microphone startup issue remains explicitly deferred. Do not import unrelated source snapshots or modify generated target definitions independently.
- Use retained owning compile/link policies and immutable source/command/dependency/object/image packets. Source work starts only after the existing capture implementer yields and its required gates permit a new owner.

## Review Focus

1. Wide output must not read beyond256bytes even when no terminator exists.
2. A shorter replacement or cleared notification must not display stale trailing bytes.
3. A narrow/zero/negative console-column count must not draw beyond its logical width.
4. Signed/high-bit character values and existing font/coordinate behavior must stay unchanged for the visible prefix.
5. Expiration, multiple notify lines, debug drawing and chat rendering must retain their existing behavior.

### Task 1: Bound the actual notify loop and qualify owning images

**Files:** Modify only the notify loop in `GoldSrc-SDK/engine/console.cpp`; add `tests/native/console_notify/` actual-source extraction/tests/runner and minimal immutable owning packet tooling. Reuse latest reviewed native engine/launcher predecessor without stale consumers. Controller updates documentation/live observations separately.

**Interfaces:** Preserve `void Con_DrawNotify()` and `char g_szNotifyAreaString[256]` exactly. Use a local descriptive capacity derived from `sizeof(g_szNotifyAreaString)`; no public interface or structure change.

- [x] Step1: Write actual-source behavior cases using the whole function and named draw/time/global services. Cases: widths1920,2560,3840; empty and short messages with stale nonzero suffix; exactly full unterminated buffer; narrow/zero/negative column count; high-bit bytes; expired notifications and independent chat/debug calls. Assert only intended visible-prefix draw calls with unchanged font/position, and unchanged timing/line behavior. Preserve original ASan overread and original stale-suffix behavior as RED.
- [x] Step2: Apply the minimal loop bound and terminator check, short-circuiting bounds before any array access. Use scoped C++ style/braces and derived capacity. Do not rewrite surrounding console/formatting/localization code.
- [x] Step3: Run Release/Debug and ASan+UBSan actual-source checks; demonstrate GREEN for the same original-source failures. Record exact source extraction SHA, command/flags, output and test counts.
- [x] Step4: Compile actual console.cpp HW/SW Release/Debug and full-link coherent native engines/launcher against the latest reviewed predecessor using retained policies, genuine-main bootstrap and complete dependency closure. Preserve old packets. Report exact Windows Win32/x64 Release/Debug owning/fixture needs for the controller; no warning suppression or live staging.
- [x] Step5: Self-review, freeze immutable evidence, commit only task paths and yield. Controller independently reviews, qualifies Windows, then stages the exact tuple and observes CS1440p system notifications. Return report with original RED, final GREEN, pins and limitations. No source/component result alone closes visual or parent migration gates.
