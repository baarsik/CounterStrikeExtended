# Live Localization Implementation Plan

> **For agentic workers:** Use independent engine, shared-UI and game-client ownership with root integration and fresh final review. No worker subagents.

**Goal:** Live language switching in CS/HL menus and HUD without restarting or losing the session.
**Architecture:** Engine-owned queued frame-boundary coordinator and revision; versioned C-table factory interfaces; presentation-only client refresh; stable localization/token and message ownership.
**Tech Stack:** Existing C++17 engine/VGUI1/VGUI2, native preferences/filesystem, CMake, Python/native fixtures.
**Spec:** docs/superpowers/specs/2026-11-13-live-localization.md
**Base:** 538661e in existing isolated codex/bundled-translations worktree.

## Constraints and interfaces

Public protocol: src/public/localization_service.h. EngineApi/RuntimeApi version1,
frame-boundary queued RequestLanguage, result/history, committed revision published
last. Existing public interface slots/object layouts stay compatible; implement
private side tables where controls need extra binding state.

- [x] Engine: precise transactional localized search-path groups preserving every
  unrelated mount/order/handle; coordinator/startup/persistence/frame/factory;
  safe prepared title snapshots with English fallback and catalogue classification;
  provider registration/surface-font-glyph handling; native isolated state tests.
- [x] Shared UI: logical catalogue requests + non-destructive prepared reload,
  stable token indices/pointers with bounded storage; focused font refresh preserving
  handles/borders; revision-aware token/variable/text/layout in both controls copies;
  GameUI dynamic presentation and Options request/status flow; all ten notices/docs.
- [x] CS: service binding/lifetime, typed owned HUD source refresh and presentation
  updates for private VGUI2 HUD/menu/scoreboard/spectator/map views, preserving all
  live state. Focused timing/identity/ownership tests and owning source membership.
- [x] HL: engine API borrowing/lifetime, typed HUD/menu/status/center sources,
  VGUI1 scheme/menu/text-window/scoreboard refresh without viewport Initialize or
  network commands; focused literal/snapshot/timing/state tests.
- [x] Integrate, regenerate CMake-derived data, run owning builds and appropriate
  native/sanitizer/actual UI tests; demonstrate repeated live switching in CS and HL
  active games; fresh review/fixes.

Delivery uses the validated source commit. App manifests, startup/signature/hash
checks and paths are recorded by `build/live-localization/delivery/` after that
commit is created; the delivery manifest identifies the exact source revision.
