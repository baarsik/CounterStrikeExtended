# Counter-Strike Timer Colon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the Counter-Strike timer colon at the size and baseline used by the surrounding HUD digits.

**Architecture:** Keep the current bitmap digits, timer arithmetic and measured legacy HUD font. Draw the colon through the existing explicit-font character API, using the font that supplies its vertical metrics. Cover both round and career timer paths without changing the engine-wide credits font.

**Tech Stack:** Existing C++ client, VGUI2 font handles and unchanged engine callback ABI; AppleClang ARM64 and retained Windows x86/x64 compile policies.

**Spec:** The user's screenshot and clarification that the `:` between digits looks wrong. Source-backed diagnosis: `health.cpp` positions the colon using `m_iFontEngineHeight`, measured from `_font` (`Legacy_CreditsFont`), but `DrawHudString` reaches `VGUI2_DrawStringClient`, whose `_creditsFont` selects scaled `CreditsFont`. The owned Valve scheme enables scaling and selects 40 pixels at 1440p while the legacy font remains 20 pixels. Actual-source RED must establish the mismatched draw before editing production. User wants the original HUD appearance, not new digit shapes or countdown behavior.

## Global Constraints

- Preserve countdown, panic colors, clipping/hide behavior, stopwatch and digit sprites, digit spacing logic, career fade and draw return values.
- Do not change the global credits font, scheme assets, generic string drawing, engine callback signatures, public interfaces, protocol or save formats.
- Use the existing `pfnVGUI2DrawCharacterAdd(int, int, int, int, int, int, unsigned int)` callback and the same legacy font handle used for layout. Its return is a width; preserve the following digit's absolute x position correctly.
- Keep production edits limited to `GoldSrc-SDK/cstrike/cl_dll/health.cpp`, adding a focused fixture under `tests/native/hud_timer/`. Any necessary declaration must use the existing actual HFont type and linkage; no new public API or production helper purely for tests.
- Preserve all reviewed input, cursor, bootstrap and audio work. Microphone startup permission work is explicitly deferred by the user and outside this task.
- The controller owns live applications, settings backups and VM1; the implementer operates only the existing clean isolated source worktree and new immutable artifacts. No original Steam asset writes, device/system settings or permissions changes.
- One production implementer at a time. Start this task only after music source/fix ownership has been yielded. Apply root C++ conventions to edited code, without broad reformatting.

## Review Focus

1. Round and career colon draws use the same font whose height supplied their baseline at 480p, 1080p and 1440p.
2. A colon callback returning a width advances from the incoming absolute x once, with correct next-digit and closest-right positions.
3. Expired and low-time timers retain countdown values and panic color behavior; career fades remain intact.
4. Hidden timer, spectator and planted-bomb early exits still suppress the appropriate round timer drawing.
5. Reinitialization/font-handle changes use the current layout font, without caching a stale handle or changing unrelated message/credits rendering.

### Task 1: Repair both actual timer colon draws

**Files:** Modify `GoldSrc-SDK/cstrike/cl_dll/health.cpp` only. Add `tests/native/hud_timer/` CMake/extractor/behavior fixture/README as needed, following existing actual-source fixture conventions.

**Interfaces:** Consume the existing legacy `_font` HFont selected by `CHudMessage::VidInit`, measured in `CHud::VidInit`, and unchanged `gEngfuncs.pfnVGUI2DrawCharacterAdd`. The round and career `Draw(float)` signatures remain unchanged. Do not add an engine/client interface.

- [ ] Step 1: Inspect actual complete round/career Draw methods, the legacy font declaration/initialization/metrics and the existing generic engine drawing path. Preserve a source manifest. Compile and run the original methods with named sprite/font/draw seams to reproduce a colon drawn through a different font than its baseline. Pin the high-resolution scheme evidence separately; do not claim a live font measurement that was not taken.
- [ ] Step 2: Add behavioral coverage for the five Review Focus items using actual complete Draw bodies and actual owning declarations where feasible. Record the original-source RED command/output. Keep a small cause-specific fixture; do not duplicate production decision logic in a test model.
- [ ] Step 3: Replace only the two colon draws with the explicit-font API, using current legacy layout font and preserving callback-width accounting. Use existing types/linkage; do not change digit sprites or add visual offsets to compensate for the wrong font.
- [ ] Step 4: Run covering Release/Debug tests and relevant sanitizer checks. Compile the changed source under the actual Counter-Strike client owning recipe and link native ARM64 Release/Debug clients with unchanged dependencies and policies. Report exact changed source/image hashes and Windows owning compile needs; controller coordinates VM1. Test output establishes behavior and builds, while actual 1440p visual acceptance remains a separate gate.
- [ ] Step 5: Self-review, commit only task source/tests and write a report with diagnosis, RED/GREEN, exact commands/output, changed paths and artifact pins. Preserve prior frozen packets and runtime images. Controller requests independent task review before integration/staging and visual acceptance.

The controller selected the narrow explicit-font repair under the user's existing autonomous migration authorization. Changing the global credits font would also change unrelated text; altering sprite assets would replace the original HUD appearance. No new approval pause is needed for this scoped bug repair. This plan does not claim the migration complete.
