# Native Server Browser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore an operable in-game Find Servers browser for native Counter-Strike and Half-Life.

**Architecture:** An owned typed Steam query/model/config component feeds an internal Servers IVGuiModule within GameUI. The existing module lifecycle and installed VGUI2 resources remain the presentation boundary; Win32 keeps its external module behavior.

**Tech Stack:** Existing C++17/native64 GameUI/VGUI2/Steam provider, pinned Valve source-sdk-2013 headers, SDL per-user preferences and retained Windows/native build policies.

**Spec:** `docs/superpowers/specs/2026-11-10-native-server-browser-design.md`. Independent builder questions and their two controller resolutions are recorded in the spec and `build/macos-runtime-bringup-20261109/server-browser-investigation-v1/builder-questions.md`.

## Global Constraints

- Existing19 retained runtime responsibilities and public IVGuiModule/engine/client ABI, saved fields/protocols/layouts remain unchanged. Win32 retains external Servers behavior.
- Only pinned typed Steam declarations; retain the exact upstream baseline/notices and explicit source-patch provenance. The controller-admitted printf signedness correction is limited to the inline address formatter, with unchanged declarations/layout/vtables/encoded data. No guessed vtable, foreign architecture binary or network-parser replacement.
- No global setting, permission, firewall, audio route, renderer/input/keyboard/capture change; microphone startup consent remains explicitly deferred. No VM2.
- Controller alone owns root/live apps/VM1. One source implementer and at most two compiler processes; start only after prior source owner yields and predecessor gates permit handoff.
- New browser persistence uses only its own safe per-user VFS mount; do not repoint shared CONFIG or write original Steam/app-bundle assets. Preserve prior config/favorites/history.
- No retained provider result pointer or callback into a deleted panel/unloaded image. Cancel/release requests before callback/Steam/GameUI retirement; preserve lifecycle processing and actual shutdown failures.
- Manifest/translator-owned source lists stay synchronized. Use immutable source/command/dependency/object/image receipts and actual owning flags; no warning-policy weakening or stale object reuse.

## Review Focus

1. Refresh/tab/stop/close or a synchronous callback may invalidate pending work; stale results must not reach a newer owner or deleted panel.
2. Ten thousand asynchronous rows and partial failures must preserve gameplay throughput, stable selection and honest state without hidden truncation.
3. Remote/control-character text and query/connection-port endian differences must never become commands, format strings or wrong-server connections.
4. Favorites/history/config/password lifecycle must preserve preexisting user state, avoid secret logging/persistence and keep browsing usable on write failure.
5. Restart, failed initialization and GameUI/Steam teardown must retire callback/message/tick ownership before dependent code unload, including reentrant requests.

### Task 1: Add the unused typed native query/model/config owner

**Files:** Add `GoldSrc-SDK/GameUI/native/ServerBrowserModel.{h,cpp}` and `ServerBrowserConfig.{h,cpp}`; add exact pinned `isteammatchmaking.h`/`matchmakingtypes.h` plus provenance in the retained Steam header location. Update only required GameUI manifest/source generation and focused `tests/native/server_browser/` component/owning packet tools.

**Interfaces:** A private `cse::server_browser` namespace owns browser list-kind/filter/row/update values, typed borrowed Steam services, model and config owners. Concrete class/method/state signatures and close/quiescence semantics require a controller ruling before production edits. No UI activation or public factory is exposed in this task.

- [x] Step1: Inspect actual provider/context, VFS/pref paths and GameUI/Steam/module teardown. Propose minimal private typed model/config boundaries, exact address/port and result-copy conversions, main-thread request/generation/reentrant-callback protocol, bounded update work and explicit BeginClose/quiescence behavior. Identify any load-bearing host retirement handshake needed before UI admission; controller rules before edits.
- [x] Step2: Add actual implementation tests for all six list kinds, refresh/quick-refresh/stop, filter/sort/selection updates, partial/failing results and10000rows; numeric addresses/ports/high-bit/control strings; favorites/history preservation; config missing/read-only/unknown fields/atomic failure; synchronous/stale callbacks, cancellation, request replacement and callback-live shutdown. Preserve original missing-native-owner RED separately from real missing-UI/runtime evidence. Tests execute real decision/model bodies through named provider/filesystem seams.
- [x] Step3: Implement the approved owner. Copy provider data while valid, distinguish query/connection ports, cancel/release every request, batch row updates and expose explicit unavailable/progress/error state. Add browser-only SDL preference path and VFS mount, error-safe persistence and header provenance. Do not start a production request or alter the live menu yet.
- [x] Step4: Run Release/Debug/sanitizer checks and actual native owning GameUI compiles/link under retained policies, plus precise Windows qualification needs. Provide a video/audio-free actual-provider harness using this real model for controller discovery/cancel/release qualification. No new production callback pump/thread or live favorites/history writes.
- [x] Step5: Freeze exact source/flags/outputs/dependencies/images, self-review, commit task-only paths and yield. Controller independently reviews and exercises real native discovery/retirement before UI integration; capability rows alone are not connection/auth acceptance.

### Task 2: Integrate the internal module and complete native browser UI

**Files:** Add `GoldSrc-SDK/GameUI/native/ServerBrowserModule.{h,cpp}` and focused dialog/page files as needed; modify `GameUI/VGuiSystemModuleLoader.{h,cpp}`, minimal actual GameUI lifecycle/message owners and manifests/translator outputs. Use installed platform/servers resources/localization; add missing project-owned resources only when demonstrated. Extend `tests/native/server_browser/` real VGUI/module/host/owning fixtures. Engine-private retirement call sites may change only after controller ruling proves a load-bearing requirement; public virtual/table ABI stays unchanged.

**Interfaces:** Consume Task1's approved typed owner. Provide the existing `VGuiModuleServerBrowser001` through a private static factory registered as an explicitly internal module record. Concrete factory/record/host-quiescence table signatures and scope require a pre-edit controller ruling. Internal records must not fake a dynamic-library handle.

- [x] Step1: Propose exact native selection, factory/loader metadata, GameUI startup/parent/frame/deactivation/shutdown and request/panel/tick/message retirement order. Audit actual void Shutdown interfaces and real image/Steam retirement callers: a false component close must prevent dependent destruction/unload through a private retryable host boundary. Do not rely on a leaked module reference or process exit. Controller resolves before integration edits.
- [ ] Step2: Add actual-source/VGUI/module cases for internal-versus-external factory selection, complete initialize/postinitialize/parent/activate/deactivate/frame/shutdown order, failure rollback/reopen/restart and in-flight retirement refusal. UI cases exercise every tab, filters/sort/selection, refresh/stop, details, favorites and keyboard/mouse connect actions; include all Review Focus inputs.
- [ ] Step3: Implement full retained browser UI and localized states from the spec, with bounded updates. Implement browser-owned masked password dialog using direct Cvar_Set plus validated numeric connect only; clear/restore owned state from existing ConnectedToGame/DisconnectedFromGame messages and preserve intervening user changes. History uses actual accepted connections and distinct stored query port. Do not add unsupported game launch flows or mutate other native input/audio/render owners.
- [ ] Step4: Run covering native Release/Debug/sanitizer and real VGUI component checks. Build complete actual changed GameUI/engine/consumer closures and coherent native runtime tuples with genuine-main objects and matching predecessors. Qualify Windows Win32/x64 owning sources and behavior/nonselection under unchanged policies. Freeze all image/source/command pins; no staging during implementation.
- [ ] Step5: Self-review, commit task changes and yield for independent scoped review. Controller stages only reviewed complete candidates and validates both games' list/filter/sort/refresh/stop/details, owned LAN/VM1 discovery, connect/gameplay/disconnect, close/reopen/focus/fullscreen/restart and hardware/audio behavior. Preserve settings/data and use controlled providers or reversible protected entries for favorites/history testing. External authentication, physical/performance and distribution/parent migration remain separate gates.
