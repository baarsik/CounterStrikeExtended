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


### Task 3: Restore retained Windows policy for the extracted loader fixture

**Files:** Modify only `tests/native/server_browser_ui/CMakeLists.txt`; evidence belongs to `build/server-browser-windows-fixture-policy-20261111/`. Production sources, manifests, headers, fixture bodies and all prior packets remain unchanged.

**Interfaces:** Consume the actual Task2 loader body and existing `browser_loader_owner_tests` in both full native64 and `CSE_BROWSER_UI_REFERENCE_LOADER_ONLY` configurations. Preserve the existing `CSE_BROWSER_SOURCE_ROOT` immutable-owner selection. Produce the original GameUI `_CRT_NONSTDC_NO_WARNINGS` definition only for the MSVC compilation of `loader_owner_tests.cpp`; retain `/W4 /WX /EHsc`, Release `/O2`, Debug `/Od`, standard/CRT/FP/RTTI/include policy and all test names. Native compile policies and all production recipes stay byte-identical. This is a downstream fixture-portability deliverable routed after Task2's five-round source cap, not a sixth Task2 source fix wave.

- [ ] Step1: Preserve the actual `fixture-0-x64-release-build_argv` RED from controller receipt `receipt-fixtures-20261111T030601Z-1852.json`: raw ZIP SHA256 `f181995aadb1ab6807e076a77b8cc18627341e0d07b23cb1c8b7394b7509e256`, raw build log SHA256 `8c2f95d8c6520e62144bbba623848b0299f2bab092eb664687cc869caad3617f`. CL rejects the unchanged extracted `VGuiSystemModuleLoader.cpp:287` `stricmp` alias with C4996/C2220. Verify all four actual GameUI owning recipes already define `_CRT_NONSTDC_NO_WARNINGS` and retain their original source/flags; no production change is needed.
- [ ] Step2: Add a source-scoped MSVC `COMPILE_DEFINITIONS` entry `_CRT_NONSTDC_NO_WARNINGS` for `loader_owner_tests.cpp` before the reference-only early-return branch. Use the existing CMake source-property mechanism; it must affect that source in both full and reference builds, and no other source. Do not disable C4996, remove `/WX`, alter global flags, rename production functions, add a generic policy framework or write spelling-only tests.
- [ ] Step3: Configure equivalent native fixture projections in fresh owned paths and compare generated compile commands to retained recipes after path normalization. Native command policy must remain unchanged. Record source-property placement and MSVC generator evidence if a supported local toolchain is available; otherwise explicitly leave actual MSVC GREEN to root. No synthetic Windows success claim or width spoofing. Existing actual Windows RED is the failing test; no mirrored new unit test is required for this CMake-only correction.
- [ ] Step4: Self-review, commit only the CMake file from BASE4408ace6f333d0234e4f695b32fd15d3955fa9a1 in the isolated worktree, and yield with exact commands/output/byte comparison. Independent scoped task review is required before root integration and Windows execution.
- [ ] Step5: Root stages a fresh immutable test-only fixture source from the reviewed commit, using existing `CSE_BROWSER_SOURCE_ROOT` to consume the exact already qualified4408 production/extraction snapshot. Keep old failed fixture directory/source/receipts unchanged. Check all consumed production/test-body bytes equal the current reviewed source; production106 owning objects retain exact recipes, tools and input proof and are not rebuilt for this CMake-only change. Run the six Windows configurations and exact72+5+72+5+1+1=156 tests under original substantive flags with only the admitted loader-source definition delta; record full generated commands/dependencies/extraction/PE/results. No R5 admission before this actual checkpoint. Subsequent full owning closure, provider/runtime/gameplay/audio/physical/distribution gates remain open.

### Task3 actual MSVC fixture follow-up

Actual firstfullfixture compile after policy correction fails C4459/C2220 because test-only anonymous globalhostResult conflicts with real extracted startup localhostResult. Production106 unchanged/GREEN. Task3fixround1 narrows repair to the four test identifier occurrences in startup_owner_tests.cpp (fixtureHostInitResult); preserve all behavior/assertions/generated flags and actual production bodies. Source handoff/review precedes fresh replacement156 qualification; e3/v3failed cohort is retained, not acceptance. Detailed scope/RED in SDDtask-3-fix-round-1-ruling.md.

### Task3 fixround2 actual owning-policy restoration

Complete Windows keep-going RED has10failedCPProws:8optionalfixtureCRT-deprecationfailures plus2entryfixtureC4535. Restore existingproduction_CRT_SECURE_NO_WARNINGS onlyonconnection/details/modelretirement targets and actualsys_dll2/EHa onlyontwoentrytargets, preservingW4/WX andallotherpolicies. Rulingtask-3-fix-round-2-ruling.md governs exactscope. Production106 unchanged; failed3ee/op4 neverqualifies156. Actual freshsixconfigs/strictsecuredefinitionandfinaleffectiveEH audit remainrequired; noTask2sixthsourcewave.
