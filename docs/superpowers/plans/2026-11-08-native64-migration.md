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

- [ ] Pointer-width command/VGUI handles and native string offsets with unchanged identity/mutability/lifetime.
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

2026-11-08, branch `codex/native64-migration`, starting at
`2c55b9054bd6a73b5a2fadd8ef4eadd62d85a3eb`. Work continues in the current checkout
as requested. The Win32 default/output contract remains unchanged.

- Immutable committed source reference preserved (3,225 files), SHA256
  `a7ed4737bc398c4df41ab0789032b984e5efb5080c1831396d9b4a1cb16e2227`.
- Original Ninja reference failures are retained: unintended analyzer enablement,
  music worker stack budget, assertion annotations and invalid UAC XML quoting.
  The maintained compatibility source fixes those failures. All 19 Win32 targets
  build in Release and Debug; static inventories contain 21 x86 PE files each.
  Raw build outputs still report unresolved CRT/provider imports explicitly.
- SDL3 3.4.20 and mpg123 1.33.7 source builds, actual machine checks and bounded
  provider/decoder probes pass Windows x64, Windows ARM64 and macOS ARM64 in both
  configurations. Windows x64 execution is under ARM Windows emulation on VM1.
- Equivalent pinned vcpkg recipes match 120 retained recipe hashes. All selected
  Windows x64 and macOS ARM64 packages built from source in both configurations;
  static audits checked 28 and 32 native archives respectively. macOS Release
  and Debug standalone compression/font/media link fixtures pass. The macOS
  packages were rebuilt with an explicit 14.0 deployment triplet after detecting
  inherited host minimums; all 32 archives now pass per-object minimum checks.
- A real POSIX worker crash was reproduced using poisoned constructor storage and
  fixed by initializing its command counts and members. macOS music tests pass
  4/4 in both configurations, including exact PCM comparison with Win32. Initial
  NEON/FMA mismatches are retained as evidence; ARM now uses scalar decoding with
  FP contraction disabled. This is one decoder fixture, not all audio parity.
- An unchanged official Win32 ReHLDS 3.15.0.896 / Metamod-R 1.3.0.149 /
  AMXX 1.9.0.5303 fixture runs on loopback using SteamCMD `steam_legacy` app90
  build5433925. The rebuilt Win32 client authenticated with Steam/VAC enabled,
  downloaded resources, returned menu/cvar/custom-message-following acknowledgments
  and recorded live movement. The initial custom-message plugin registration
  failure was fixed in the owned test plugin; server package binaries remain
  unchanged. Full voice/reconnect/changelevel/provider acceptance remains open.

- The new descriptor/validator and owned module-loading fixture pass on macOS,
  Windows x64/ARM64 and Win32 in Release/Debug, including mismatch rejection before
  callbacks and high-address strings/handles. Production loaders are not wired yet.
- `game_controls.lib` correspondence audit found different candidate source bytes
  and behavior. Debug's matching PDB proves no archive objects contribute to HL;
  Release CRT/import/source evidence supports the same. HL already compiles its
  VGUI1 controls; CS already compiles its owned VGUI2 controls. The unused HL link
  is removed through the retained manifest, ordinary R/D links and HL gameplay recording/replay pass after removal.

Task 1 and Task 2 gates remain open: expanded save/navigation/render fixtures,
complete reference qualification, generated native runtime policies and production
module ABI integration still need work. Tasks 3–7 remain
unqualified. No complete native game build, renderer, Steam/CEF parity or target
promotion is claimed. Detailed build/failed-run evidence lives in the task ledger
and the isolated `cse-native64-20261108` artifact directory, not default runtime
outputs.

The latest review reproduced and fixed incorrect Debug import mapping and cached
foreign dependency headers. Positive native package fixtures pass; dedicated
regressions reject both mappings/fallbacks and foreign headers. Build tools tests
pass 39/39 and qualification tools tests 77/77 on the current foundation snapshot.
The Win32 Debug music fixture exposed a 53 ms sequential wall-clock calibration
variance. Its completion test now uses actual SDL device sample counts, with one
mixer block of phase allowance; the producer and decoded bytes are unchanged.

Early provider inventory has pinned official Windows x64 Steam artifacts and a
Valve-signed installed Mac ARM64 candidate matching the legacy initialization
exports. Required generic interface lookup, real auth/voice and lifecycle still
need standalone runtime evidence. Official CEF156.0.26 artifacts for Windows x64
and macOS ARM64 are downloaded and independently SHA256-pinned; integration is
pending. The shared/CS HTML serializer declarations differ on non-Windows and
must be consolidated or adapted explicitly before a native Mac provider is safe.

The first Task 3 component is a module-local typed callback registry and field
operations core. Release/Debug standalone fixtures pass Win32, Windows x64,
Windows ARM64 and macOS ARM64: eager catalogs, all four game member signatures,
full adjusted member pointers, high-address objects, typed null handling, owned
symbol names, case-sensitive aliases and explicit ICF ambiguity handling. The
five real fields per game and generated production catalogs are not integrated
with it yet; existing save/restore still uses its legacy path.
