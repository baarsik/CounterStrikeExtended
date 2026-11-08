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
- **Module ABI/callbacks:** runtime ABI version 8 guards the actual engine/game
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
  Native symbol addresses now survive export-table construction and both lookup
  callbacks; POSIX modules use native handles and direct symbol discovery.
  Canonical-name conversion is bounded, and prediction setters retain complete
  typed member pointers. Native fixtures execute actual exported functions and
  distinguish addresses with equal low 32 bits. Updated Win32 export tables,
  game-module probes and CS/HL save/record/replay pass after integration.
  Other module boundaries and complete native game loading remain open.
- **Native game modules:** the explicit Windows component entry builds the
  retained CS/HL server targets from their existing generated source selections
  and per-file policies. x64 and ARM64 Release/Debug DLLs pass machine inspection,
  export parity, actual ABI/save-field/world/C4 probes and high entity-offset
  checks. Explicit C ordinals and canonical C++ export coverage are preserved.
  Private-entity allocation byte counts are native-width; legacy index/RNG/save
  counts and navigation records keep checked 32-bit representations. Autobuy
  appends preserve their terminator and adjacent storage. All 19 Win32 targets
  and the full save/record/replay regression set pass with producing hashes
  checked. x64 fixture execution used VM1's x64 emulation; these module checks
  do not qualify a complete native game or native x64 hardware execution.
  macOS ARM64 Release/Debug game libraries now build from the same generated
  source inventories. Their precise FP/contraction, per-file EH, RTTI and assertion
  policies are explicit; narrowing diagnostics remain fatal. ARM64/Mach-O minimum
  14.0, every reference C/canonical C++ export and actual ABI/save/world/C4 probes
  pass. Two exported turret methods have out-of-line bodies so their exports remain
  present without a base vtable. Portable assertions, directories, text entry points
  and a native-width epoch counter have bounded fixtures. These component checks
  do not qualify a native dedicated server or full native savegame round trip.
- **Save/data representations:** native storage facts govern strides, clearing
  and emptiness; integers retain four-byte encodings and strings remain symbolic.
  Raw-pointer descriptors reject at compile time and runtime before pointer bits
  can be written or restored. Unknown legacy fields remain skippable.
  Level and global-header loaders share a checked native arena with aligned
  entity rows, bounded token decoding, exact reads and allocation-failure
  cleanup. Save creation uses the same allocator. Legacy level/global-header
  imports pass across the architecture matrix and Win32 load/save/reload passes;
  these imports do not establish complete native game save round trips.
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
- **Native effects boundary:** native Windows/POSIX SDL DMA uses the shared
  stereo transfer with locked writes, unchanged clipping/volume/wrapping and
  native VOX word pointers. All six Windows configurations and macOS ARM64
  Release/Debug pass seven bounded checks, including the actual DMA source and
  real SDL provider. Mac sanitizers pass. Twenty-six PCM outputs match the frozen
  transfer code exactly; injected byte corruption and missing locks are detected.
  The maintained Win32 device stays DirectSound/WaveOut; all 19 targets and the
  six save/gameplay/replay regressions pass with producing module hashes checked.
  These checks do not close native full-mixer/DSP, hardware-rate conversion,
  microphone/voice, A3D or whole-game audio acceptance.
- **CPU/profiler boundary:** production CPU and fasttimer sources pass Windows
  x86/x64/ARM64 and macOS ARM64 Release/Debug fixtures; Mac sanitizers pass.
  ARM64 uses matching native timer units and does not advertise x86 instructions.
  Native topology is bounded to legacy byte counts. Wrong timer units and count
  wrapping are detected by injected faults. Win32 detection/calibration remains
  the reference. Engine instruction dispatch, scalar conversions, FP control and
  full numerical/gameplay acceptance remain open.
- **Scalar conversion boundary:** binary32 scalar conversion matches the actual
  SSE instruction for NaN, overflow, subnormals and finite truncation; thirteen
  conversion corpora match exactly across the participating configurations.
  Actual angle/cvar bodies retain their separate FP policies, wrapping/division,
  hooks and formatting. Frozen and updated Win32 results match per configuration;
  Mac sanitizers and six updated Win32 runtime regressions pass. A pre-existing
  emulated Win32 Debug remainder difference for extreme angles remains recorded
  explicitly; complete numerical/gameplay acceptance remains open.
- **Packed lighting boundary:** native integer lighting matches the frozen Win32
  MMX RGB555/RGB565 function exactly in eleven pixel/state corpora. All four mips,
  signed palette operations, saturation, wrapping, disabled behavior, guards and
  native high pointers are covered. A checked-in reference hash detects an
  injected color-mask defect; Mac sanitizers pass. Win32 keeps its original MMX
  path, and native dispatch retains the packed-lighting control. All 19 Win32
  targets and six save/gameplay/replay regressions pass with producing hashes
  checked. Presented scenes,
  complete rendering and SDL_GPU acceptance remain open.
- **Native loader boundary:** shared owning/borrowed loading and symbol lookup
  preserve per-owner Windows main-executable fallback policies. Native suffixes,
  high addresses, data ordinals, repeated ownership and hidden-image visibility
  pass actual-source fixtures across Windows x86/x64/ARM64 and macOS ARM64
  Release/Debug; Windows pinned-reference comparisons pass. Mac sanitizer drivers
  and loaders pass with only the external reloadable leaf uninstrumented, after
  an independent direct-loader ASan ODR reproduction. Real providers, other module
  ABI guards and complete native runtime startup remain open.
- **Steam binding/client provider boundary:** pinned real Windows x64 and macOS
  ARM64 providers pass Release/Debug client sessions through owned atomic export
  binding; Mac ASan/UBSan passes. Two init/shutdown cycles, seven exact interfaces,
  SDK context caching, licensed Half-Life identity and actual async call-result
  delivery pass. Rejection tests and a partial-table defect mutation pass their
  expected gates. Fifteen images and producing inputs are checked. Decoder-rate
  availability is verified; captured/encoded voice, native server/authentication,
  all-consumer service routing and legacy external-server acceptance remain open.
- **Owned Steam routing/lifetime boundary:** all five SDK consumers select the
  owned native entry points through generated architecture policy; Win32 keeps
  its original import. Real CS/App10 and HL/App70 EXE/peer-DLL fixtures pass on
  Windows x64 and macOS ARM64 Release/Debug, plus full Mac ASan/UBSan. Twenty cases
  cover 110 real SDK client cycles, high callback owners, SDK helpers and global
  cleanup; three peer reload cycles per case keep a remaining consumer usable.
  Actual tests exposed and fixed POSIX weak cache/inline-code coalescing and late
  global teardown ordering. Thirty fixture images, 42 Win32 images and producing
  inputs are checked; all 19 Win32 targets and six matching-binary regressions
  pass. Pin/staging checks restore missing output and reject same-timestamp
  content changes, including Windows UNC builds. Native server authentication,
  encoded voice, general pre-callback module ABI checks and complete native
  runtime acceptance remain open.
- **Legacy Steam authentication boundary (Windows x64):** intact production
  connection/ticket/disconnect definitions authenticate with the unchanged secure
  Win32 ReHLDS fixture in Release and Debug. Each run generates a real 206-byte
  ticket, receives connection acceptance, observes no SDK denial for fifteen
  seconds, and matches a fresh server `STEAM USERID validated` line. Thirty-two
  server images retain their pinned hashes. Packet/error fixtures pass nine
  configurations across Windows x86/x64/ARM64 and macOS ARM64, including Mac
  sanitizers and native high addresses. Failure tests exposed and fixed empty,
  negative and oversized SDK ticket handling without changing valid packet bytes.
  Thirteen fixture images and producing inputs are checked; all 19 Win32 targets
  and six save/gameplay/replay regressions pass. Windows x64 execution uses VM1
  emulation. Mac authentication, netchan sign-on, native gameplay, encoded voice,
  consistency/downloads and AMXX message-reception acceptance remain open.
- **Legacy Steam authentication boundary (macOS ARM64):** native Release/Debug
  clients authenticate directly across the private Mac/VM link with a separate
  unchanged secure Win32 ReHLDS/Metamod/AMXX fixture. Each receives a real 206-byte
  ticket and matches a fresh server Steam identity approval. The server copy's
  32 image hashes remain pinned; its firewall rule limits the peer to the Mac.
  The updated explicit-endpoint fixture also requalifies Windows x64
  authentication and all nine packet configurations. Thirteen image identities
  and producing inputs are checked. Production sources/build policies are
  unchanged from the preceding six-case Win32 regression. Native netchan sign-on,
  gameplay, voice, consistency/downloads and AMXX message reception remain open.
- **Native Steam server transport/lifetime boundary:** Windows x64 and macOS
  ARM64 Release/Debug owned modules pass anonymous server and licensed
  listen-server sessions for CS/App10 and HL/App70; Mac sanitizers pass. Twenty
  server cases cover 120 secure SDK sessions and three complete module reloads
  per case. Twenty client regressions cover 110 SDK cycles; listen-server cases
  add 30 client cycles. A Mac-only anonymous transport reference prevents the
  pinned SDK's repeated standalone initialization/exit crash without client login
  or unauthenticated game users. The reference closes before its module/provider
  unload. Nine offline fixtures cover atomic export binding, failure recovery,
  high opaque pointers and independent owners; a duplicate-pipe mutation is
  detected. Seventy-two image identities, producing inputs, all 19 Win32 targets
  and six matching-binary runtime regressions pass. Direct SDK controls reproduce
  remaining WebUI restart diagnostics. Complete native game/server protocol,
  voice, browser and renderer acceptance remain open.
- **Browser interface boundary:** the owned native controller is an abstract
  interface, and both game UIs use the same 25 response callbacks and 28 serializer
  methods. Their ordered signatures match the immutable Windows reference.
  Pointer-width handles, UTF-16 code units and supplementary character values
  pass nine offline configurations, including macOS sanitizers. All 19 Win32
  targets and six fresh matching-binary save/gameplay/replay regressions pass.
  These checks do not execute CEF or qualify browser behavior.
- **CEF SDK boundary:** pinned stable API 15600, Windows x64 and macOS ARM64
  Release/Debug pass four standalone fixtures covering eight browser process
  lifetimes. Exact software BGRA pixels, resize, mouse/Unicode input, JavaScript,
  persistent cookies/local storage/HTTP cache, cookie flushing and complete helper
  shutdown pass. Forty binary identities and selected producing inputs match.
  Windows uses CEF's sandbox bootstrap at normal user privilege; the Mac app and
  five helpers build with command-line tools and target macOS 14. Tampered API
  headers are rejected. This is SDK evidence; owned HTML controller integration,
  MOTD and browser media behavior remain open.
- **Browser process transport boundary:** the prepared SDL3 process channel and
  fixed-width codec pass nine architecture/configuration fixtures, including Mac
  sanitizers. Four additional Windows ARM64/x64 pairings pass in both directions.
  Tests cover exact bytes, fragmented/invalid streams, simultaneous large
  transfers, Unicode arguments, failure recovery and draining queued commands
  before cooperative shutdown. These sources are not yet selected by runtime
  manifests or connected to the owned HTML controller/CEF host.
- **Providers/platform/renderer:** stable CEF156.0.26 archives are pinned for
  integration. Complete provider acceptance and browser behavior remain pending,
  as do the portable platform/text/audio layer, shared SDL_GPU renderer and macOS
  development bundle.
- **Network worker boundary:** cooperative stop, UDP wake and join replace forced
  termination on Windows and supply the missing POSIX worker. Socket replacement
  stops the worker before closing descriptors. Controlled real-UDP queue/lifetime
  fixtures pass Windows x86/x64/ARM64 Release/Debug under both socket import
  policies, plus macOS Release/Debug/ASan/UBSan/TSan. This does not qualify inherited
  clock/cvar/RNG/diagnostic/FakeIP concurrency, lag/split packets, netchan or external
  server play. The Win32 reference retains its existing receive algorithms.
- **macOS font discovery boundary:** CoreText selects native UTF-8 family or exact
  PostScript faces and verified FreeType collection/instance indices. Existing
  glyph rasterization/effects remain in FreeType. Native covering, readable
  fallback handles CJK and supplementary scalars without Linux font-name remaps.
  Twelve discovery/manager cases pass Release, Debug and sanitizers; actual UI
  layout/metric parity, custom Surface registration and macOS 14 execution remain
  open. Platform-only membership is owned by the retained manifests and translator.
- **Platform regression reference:** all 19 Win32 targets pass Release/Debug,
  followed by six fresh save/gameplay/replay cases with 14 matching producer-module
  hashes each. All 42 PE machine types, imports, exports and resource payloads are
  checked; resource payload comparison accounts for relocated PE resource RVAs.
  These receipts cover the immutable platform snapshot, before subsequent browser
  and custom-font integration changes, and do not promote a full native target.
- **Custom-font ownership and browser keys:** a single manifest-owned macOS
  registration dylib serves both Surface implementations. Ten real CoreText
  cases pass Release/Debug/sanitizers, including two independently unloaded
  consumers, external registrations, long Unicode VFS paths and failed-release
  retries. A reproduced teardown-order defect has a failing old-code regression;
  the service now initializes before its clients. Native HTML adapters explicitly
  normalize all 107 VGUI key codes and preserve keypad location; maintained
  Win32 adapters keep their existing path. Six Windows key configurations and
  three Mac configurations pass. A fresh 3,386-input snapshot passes all 19
  Win32 targets, all 42 PE/resource checks and six matching-module runtime cases.
  Full native UI/font metrics, IME and game input acceptance remain open.

- **Owned browser controller and host:** the versioned process boundary now
  implements the existing HTML methods and typed callbacks with sandboxed CEF.
  Matching Windows x64 and macOS ARM64 Release/Debug fixtures pass navigation
  policy, real popup ownership, MOTD pages, input, pixels, persistent storage and
  cooperative shutdown. Cancellation retains stale response slots, and renderer
  geometry is validated before native conversion. Routine Mac fixtures use
  isolated real encrypted Keychain items and verify exact cleanup, avoiding
  production-key approvals after each developer rebuild. Controlled controller
  tests also pass Release/Debug/sanitizers. Runtime manifests, full game/browser
  presentation parity and actual native prompt/file dialogs remain open.
- **Native browser dialog ownership:** real per-browser native parents now
  survive OSR browser teardown and release on the UI thread. macOS Release/Debug
  pass prompt accept/cancel, owned-file selection/cancel, browser removal during
  a prompt and EOF during a file dialog, plus existing protocol/frontend tests.
  Windows Release/Debug pass the protocol/frontend regressions and show the
  default native prompt. Windows interaction remains unqualified because the
  computer-use input route failed; no provider defect or substitute dialog was
  inferred from that environment failure. Production integration remains open.

Detailed passing and failed receipts, source/binary hashes and scope rulings are
kept in the task ledger and isolated cse-native64-20261108 artifact directory.
No tests, symbols or qualification artifacts enter default runtime outputs.
