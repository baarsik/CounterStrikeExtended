# Native x64 / ARM64 Migration Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans task by task. This is the active implementation record for the user-approved native migration; do not substitute a green build for runtime qualification.

**Goal:** Preserve the complete retained Counter-Strike and Half-Life runtime on Windows x64 and macOS Apple Silicon, maintaining Win32 compatibility and unchanged external Win32 server interoperability.

**Architecture:** Rebuild owned modules together behind explicit native ABI and platform boundaries. Keep legacy network and disk encodings independent of host layout. Use SDL3 output/input, mpg123 music, native Steam and maintained CEF providers, and a shared SDL_GPU renderer.

**Tech stack:** C/C++, generated CMake/Ninja, MSVC, Apple Clang, SDL3 3.4.20, mpg123 1.33.7, existing pinned compression/font/media versions.

**Spec:** The user-approved “Native x64 / ARM64 Migration Plan” supplied in this chat on 2026-11-08; the constraints and stage gates below preserve its implementation requirements.

## Global constraints

- Windows 11 x64 is first and mandatory; macOS 14+ Apple Silicon is mandatory.
- Windows ARM64 is conditional on complete native/provider/gameplay qualification with available hardware or an existing qualified Parallels graphics path. An isolated x64 provider helper is permitted only where necessary and supported, with a versioned pointer-free protocol.
- Linux x64/ARM64 are opportunistic after mandatory targets; no separate renderer, provider project or hardware acquisition.
- Preserve all 19 production responsibilities, CS/HL gameplay, UI, media, dedicated servers and HLTV.
- Native clients must play on unchanged Win32 ReHLDS + Metamod + AMX Mod X. Preserve protocol 48, delta/user-command/message encodings, authentication, consistency, downloads and voice. Never bypass integrity policies.
- Preserve saves/demos/navigation bidirectionally with updated project Win32 builds; untouched Valve binary compatibility and loading x86 DLLs/blobs into native processes are excluded.
- Preserve the x86 default/root output workflow until mandatory acceptance. Native build/runtime directories remain isolated. Only then make Windows x64 default and isolate explicit Win32 reference outputs.
- No silent providers, authentication stubs, missing voice, text-only MOTD, font substitution or lost rendering/audio features.
- Keep gameplay algorithms stable. Preserve per-file FP/EH/assertion settings; no global fast math or representation-driven wire changes.
- Keep libraries, symbols, intermediates and tests outside runtime directories. Release diagnostics and distribution materials stay opt-in.
- Runtime/provider/network checks require separately authorized isolated fixtures; compilation, parsing and standalone unit fixtures do not authorize game/provider execution.
- Deliver reproducible developer builds and a local macOS development app bundle; consumer installation, publication and notarization are deferred.
- User chose the current checkout; implement on `codex/native64-migration`.

## Review focus

- High addresses and mismatched native modules must fail safely before callback exchange.
- Win32 disk strides, 260-byte paths, pointer placeholders and callback/entity symbols must round-trip across architectures.
- Restore every C4 phase, including legacy attenuation reconstruction and repeated final beeps; never dereference saved pointer bits.
- Shutdown/restart and provider failure must preserve ownership and join workers without forced termination.
- External AMXX menus/HUD/custom messages/cvar queries/downloads/consistency/voice must work without changing external servers.

## Tasks and gates

### Task 1: Immutable reference and acceptance infrastructure

**Files:** `tools/qualification/`, `docs/`, generated inventory through `tools/translate_msbuild.py`.
**Interfaces:** Produce source/package SHA-256 manifests and static runtime reports, retaining exact revision/build identity. Consume the generated production inventory, not a second hand-maintained list.

- [x] Test malformed and mixed-architecture PE/COFF/ELF/Mach-O inputs, archive members and checked manifest handling before implementation.
- [x] Preserve immutable source reference at `2c55b9054bd6a73b5a2fadd8ef4eadd62d85a3eb` and hash it.
- [ ] Qualify fresh Ninja Release and Debug for all 19 Win32 targets: layout, architecture, imports/exports/resources and staging. Preserve binaries and source hashes together. Record pre-existing failures.
- [ ] Extend existing fixtures with saves, demos, navigation, render scenes, media/input/localization and lifecycle cases.
- [ ] Pin unchanged Win32 ReHLDS/Metamod/AMXX and representative plugins without modifying external-server behavior; execute only with separate fixture authorization.

**Gate:** Reproducible reference builds and fixtures; source-only evidence cannot close this gate.

### Task 2: Architecture-aware generated build and dependencies

**Files:** `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `hardware/CMakeLists.txt`, `tools/translate_msbuild.py`, retained manifests, dependency locks.
**Interfaces:** Consume Task 1 binary inspection. Produce target OS/CPU/compiler policy and source-built native dependencies without duplicating source membership.

- [x] Add tested target detection and actual binary machine checks; reject mixed/unknown architectures independent of filenames and host architecture.
- [ ] Separate generated source membership from OS/CPU/compiler policy, including RC/exports, runtime naming and preserved per-file FP/EH/assertions.
- [ ] Add isolated Ninja Release/Debug Windows x64, macOS ARM64 and conditional Windows ARM64 presets; keep x86 default unchanged.
- [ ] Source-build pinned SDL3 3.4.20, mpg123 1.33.7 and existing compression/font/media versions/features for each target.
- [ ] Verify `game_controls.lib` source/archive correspondence; remove the proven unused HL archive dependency and retain the existing source-built CS controls (implementation ruling below).
- [ ] Verify standalone hardware entry point and bounded module-loading fixtures, including mismatches, before whole-game bring-up.

**Gate:** Correct native dependencies and bounded module fixtures, not merely successful configuration.

### Task 3: Native ABI and legacy codecs

**Files:** native shared interface headers/loaders; engine/client/game/VGUI string, handle and callback boundaries; demo/save/navigation readers and writers.
**Interfaces:** Export `CseGetModuleAbi` with descriptor/runtime versions, architecture, pointer width, compiler ABI family and exchanged-table layout signatures. Check it before callback exchange. Typed callback registries must exist before restore.

- [x] Pointer-width command/VGUI handles and native string offsets with unchanged identity/mutability/lifetime (component fixtures; whole-game native qualification remains open).
- [ ] Typed function/member-function canonical-name registries; never treat member pointers as plain addresses.
- [ ] Checked native entity offsets separate from fixed-width wire/save indices; separate native field storage from serialized widths/strides.
- [ ] Explicit Win32 demo and navigation records, fixed paths and placeholder pointers; reconstruct native pointers after decoding.
- [ ] Preserve save envelopes and symbolic entities/callbacks. Replace C4 raw sound pointer with optional symbolic phase in both updated Win32 and native writers; reconstruct legacy phase from exact saved attenuation after all fields restore.
- [ ] High-address ABI fixtures and Win32 ↔ native bidirectional round trips before gameplay tests.

**Gate:** ABI and data fixtures pass on participating builds.

### Task 4: Portable platform, audio, text and numerical behavior

**Interfaces:** Shared loading/time/workers/synchronization/settings/directories/clipboard/shell services; platform-specific native implementations.

- [ ] Preserve VFS search order, fallback/game paths and case semantics.
- [ ] SDL3 window/input, effects output and microphone capture; retain mixing/DSP/spatialization/pitch/channel behavior and mpg123 music.
- [ ] Cooperative joined shutdown including networking; preserve UDP/netchan order and semantics.
- [ ] CPU discovery and FP control by architecture; faithful scalar x86-operation replacements first, NEON only after equivalence.
- [ ] Preserve VGUI resources/widgets/layouts with explicit UTF-16 boundaries, FreeType rasterization and macOS CoreText discovery.
- [ ] Native A3D spatial/geometry equivalents and digital mappings for legacy CD track commands. Physical optical-drive support excluded.

**Gate:** Native platform/audio/input/font/lifecycle fixtures independently pass.

### Task 5: Real provider integration

- [ ] Owned Steam façade preserving authentication/session/callback/server/voice behavior; prove Windows x64 and macOS native integration early.
- [ ] Pin a maintained compatible stable CEF release and artifact hashes; implement existing HTML-controller contract, offscreen textures/input/cookies/cache/MOTD and lifecycle.
- [ ] Writable user locations for browser cache, saves and configuration.
- [ ] Conditional ARM64 native providers, or supported isolated x64 helper with explicit failure/shutdown protocol. Missing parity leaves target incomplete.

**Gate:** Real provider behavior and repeated lifecycle evidence.

### Task 6: Complete Windows x64 and macOS runtime/rendering

- [ ] Windows x64 complete runtime using OpenGL initially for comparison.
- [ ] Shared rendering commands/state through SDL_GPU: D3D12 Windows, Metal macOS, Vulkan opportunistic Linux; offline shaders.
- [ ] Explicit legacy topology/matrix/display-list/fixed-function translation.
- [ ] World/model/sprite/lightmaps/fog/transparency/decals/particles/triangle APIs/VGUI/browser/video/screenshots/MSAA/display parity.
- [ ] Keep selectable Windows OpenGL during qualification; switch default only after SDL_GPU parity with feature-preserving fallbacks.
- [ ] Native macOS module loading and local app bundle, helpers/library paths/writable-data separation.

**Gate:** Complete CS/HL acceptance in Release and Debug on both mandatory targets.

### Task 7: Conditional targets and promotion

- [ ] Windows ARM64 machine/native execution/provider/gameplay checks; defer if existing hardware/graphics cannot qualify.
- [ ] Linux only through shared POSIX/SDL_GPU/audio/providers and small build/naming/packaging work; otherwise defer per architecture.
- [ ] Keep buildable, runnable and qualified states distinct. Switch defaults only after Task 6 gate.

## Acceptance evidence

For every promoted target in Release and Debug: all runtime responsibilities and architecture/import/export checks; ABI mismatch rejection and strings/handles above 4 GiB; Win32 ↔ Windows x64 ↔ macOS demos/saves/navigation including transitions/callbacks/entities/C4; seeded movement/prediction/collision/RNG/weapons/bots/bomb/HL decisions and exact encoded bytes; renderer scenes/mixer PCM/capture/microphone/voice/Unicode/IME/cursor/focus/DPI/controllers/browser/restarts; listen/dedicated/HLTV/reconnect/changelevel/downloads/consistency and AMXX behavior on the pinned unchanged Win32 fixture. Add actual user server configurations when supplied. Compilation, static inspection, module loading, gameplay and external-server evidence remain separate.

## Current progress

Work remains in the requested checkout on codex/native64-migration. Windows x86
remains the default with its existing root runtime outputs. No native target is
promoted: complete native game builds and Tasks 3–7 acceptance are still open.

- **Reference:** committed source 2c55b9054bd6a73b5a2fadd8ef4eadd62d85a3eb
  is preserved with SHA256
  a7ed4737bc398c4df41ab0789032b984e5efb5080c1831396d9b4a1cb16e2227.
  Original Ninja failures are recorded; the maintained source fixes them.
  All 19 Win32 targets build in Release and Debug, producing 21 PE files per
  configuration. The sealed 99-file reference remains intact. Imports, module
  loading, gameplay and external-server results remain separate evidence.
- **Build/dependencies:** compiler-target detection and actual binary inspection
  reject wrong or mixed architectures. SDL3 3.4.20, mpg123 1.33.7 and pinned
  compression/font/media packages build from source for Windows x64/ARM64 and
  macOS ARM64. macOS objects and fixtures enforce deployment minimum 14.0.
  Standalone dependency, decoder and worker fixtures pass. ARM scalar decoding
  matches the reference PCM; this does not qualify effects, microphone or voice.
  The unused HL game_controls.lib link was removed after correspondence and
  PDB audits; the existing source-built CS controls remain. Generated production
  policies and complete native runtime presets are still pending.
- **External server:** the pinned unchanged Win32 ReHLDS 3.15.0.896,
  Metamod-R 1.3.0.149 and AMXX 1.9.0.5303 fixture accepts the rebuilt Win32 client
  with Steam/VAC enabled. Menu, cvar, custom-message acknowledgment, downloads,
  consistency and live movement checks pass. Native clients, voice, reconnect
  and changelevel remain unqualified.
- **Module ABI/callbacks:** runtime ABI version 6 guards the actual engine/game
  boundary before callback exchange, including eleven generated shared-layout
  profiles and single-use legacy initialization. Native and Win32 mismatch
  fixtures pass in Release/Debug; preserved older GameDLLs reject before
  callbacks. Other production module boundaries remain pending. Typed eager
  catalogs restore game member callbacks through canonical names, including
  bounded legacy aliases, without interpreting member pointers as addresses.
  Export-name discovery now bounds PE32/PE32+ headers, sections, RVAs and names
  without assuming contiguous names or a `.rdata` section. Standalone fixtures
  pass across the architecture matrix; file-mapping lifecycle tests pass on
  Windows and 42 real DLLs match MSVC's export listing. Updated Win32 engine
  save/load and CS/HL recording/replay pass with producing module hashes checked.
  Native symbol-address widths and other module boundaries remain open.
- **Save/data representations:** native storage facts govern strides, clearing
  and emptiness; integers retain four-byte encodings and strings remain symbolic.
  Raw-pointer descriptors reject at compile time and runtime before pointer bits
  can be written or restored. Unknown legacy fields remain skippable.
  C4 persists an optional symbolic beep phase and real wave counter, with exact
  legacy attenuation recovery and restore-failure cleanup. Engine/HLTV demos use
  explicit Win32 records, including 260-byte paths and rebuilt local pointers.
  HL .nod uses bounded version-16 records; CS .nav separates IDs from native
  pointers and rejects unresolved references. Cross-architecture codec fixtures,
  malformed cases and macOS sanitizers pass. Updated Win32 gameplay, legacy demo
  replay, save/load and navigation fixtures pass. Complete native game-file
  round trips and allocation-failure injection remain open.
- **Native identities:** engine strings retain native-width relative identities,
  aliases and mutable storage. Worldspawn team lists have optional symbolic save
  fields and an explicit initialized flag; older saves recover the original BSP
  setting through the existing lexer. Paired Win32 world tests cover nonempty,
  changed and explicitly empty settings. Entity byte offsets use checked native
  storage while wire/save indices remain fixed-width. Actual Win32 DLL probes
  verify entity-ID envelopes, remapping and array tails. High-address native
  fixtures cover strings, key values, offsets, alignment and wrap rejection.
- **Commands/VGUI:** command iteration preserves native pointer handles, list
  order, name identity and lifetime. VGUI panel identities are pointer-width;
  packed serial handles and scheme/font/texture/cursor IDs remain 32-bit.
  Console parenting, menu messages and browser drag targets retain full panel
  identities. Fixtures execute production VPanel parenting/message callbacks
  and serial-handle conversion at high addresses across all participating
  Release/Debug architectures; macOS sanitizers pass. Actual rebuilt Win32 VGUI
  DLLs pass 24 allocation/parenting/message/free/stale-handle cycles per
  configuration. All 19 Win32 targets, CS/HL recording/replay and legacy HL
  load/save/reload pass after integration, with loaded module hashes checked.
  The synthetic console keyboard probe also fails on the sealed reference and
  remains unqualified; it does not establish native UI, input or browser parity.
- **Providers/platform/renderer:** official native Steam candidates and stable
  CEF156.0.26 archives are pinned for integration. Real native authentication,
  sessions, voice and browser behavior remain pending, as do the portable
  platform/text/audio layer, shared SDL_GPU renderer and macOS development bundle.

Detailed passing and failed receipts, source/binary hashes and scope rulings are
kept in the task ledger and isolated cse-native64-20261108 artifact directory.
No tests, symbols or qualification artifacts enter default runtime outputs.
