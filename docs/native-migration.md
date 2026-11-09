# Native x64 and ARM64 migration

The migration preserves Counter-Strike 1.6, Half-Life, their UI/media, dedicated
servers and HLTV. Win32 remains the maintained reference and default. **No native
target has completed acceptance.** See [build commands](building.md),
[validation evidence](validation.md) and [component tests](../tests/native/README.md).

## Targets

| Target | Commitment | Current state |
| --- | --- | --- |
| Windows 11 x64 | Required; first complete native target | All 19 targets build; selected CS/HL runtime checks pass on VM1 under x64 emulation |
| macOS 14+ Apple Silicon | Required | Dependencies and several modules build; full engine/client linking is incomplete |
| Windows ARM64 | Conditional | Native components pass selected checks; full providers/gameplay remain unqualified |
| Linux x64 / ARM64 | Opportunistic | Reuse shared POSIX/SDL implementations only; defer substantial platform/provider work |

Windows ARM64 must work on available hardware or a qualified Parallels graphics
path. A necessary x64 provider helper may use a versioned, pointer-free protocol;
it must retain full provider behavior and clean failure/shutdown. No additional
renderer project or hardware purchase is part of conditional-target work.

## Compatibility requirements

- Native clients must play on unchanged Win32 ReHLDS servers running Metamod and
  AMX Mod X. Preserve protocol 48, delta/user-command/message encoding,
  authentication, consistency, downloads, menus, HUD, sounds and voice. Integrity
  policies must not be bypassed.
- Rebuild owned native modules together. Original x86 DLLs and executable blobs
  cannot load in a native process. Compatibility does not require loading their
  executable code or round-tripping files with untouched Valve binaries.
- Saves, demos and navigation must round-trip between native targets and the
  updated project Win32 build. Keep legacy envelopes, symbolic field/callback
  names, entity indices, fixed path lengths and serialized widths.
- Keep gameplay algorithms and per-file floating-point, exception and assertion
  policies stable. Native storage size must not change wire or disk encodings.
- Preserve VGUI widgets/resources/layouts, startup video, browser MOTD, music,
  effects, spatial/geometry behavior and microphone/voice features. SDL3 provides
  platform I/O; it does not replace these algorithms or providers by itself.
- Map legacy CD track commands to digital music; physical optical-drive support
  is excluded. Developer builds and a local Mac app bundle are in scope;
  installers, publication and notarization are deferred.

## Implemented boundaries

| Area | Implementation and evidence | Remaining work |
| --- | --- | --- |
| Build/dependencies | One generated inventory from retained manifests; explicit OS/CPU/compiler policies; source-built SDL3 3.4.20, mpg123 1.33.7 and pinned media/font/compression libraries; source-built CS controls | Fresh complete builds after later source edits; conditional-target qualification |
| Module ABI | `CseGetModuleAbi` checks architecture, pointer/compiler profile, role and consumed table signatures for selected game/client/filesystem exchanges; runtime ABI 10 | Other C++ boundaries and rejection before all static callbacks |
| Data | Native handles/string storage, typed callback registration, explicit Win32 demo/navigation records, symbolic C4 phase, HPAK records and bounded custom-decal parsing | Full entity/transition/C4/nav writing, VFS/spray integration and Mac game round trips |
| Platform/numerics | Owned module loading, cooperative workers, UDP components, CPU/timer discovery, scalar packed lighting and legacy conversion helpers | Complete native engine, dedicated/admin and UI platform owners; restart/unload integration |
| Audio | mpg123 music, SDL DMA/capture components, retained mixing/DSP rules and native voice-mixer component | Real device/capture/voice delivery and A3D provider/spatial parity; measured cross-CPU SDL conversion differences remain |
| Steam/CEF | Owned Steam service and sandboxed CEF controller/host; real provider fixtures on Windows x64 and Mac ARM64 | Full game/browser/voice integration and provider failure/lifetime acceptance |
| UI/text/input | UTF-16 compatibility helpers, CoreText/FreeType components, Marlett/font probes, keyboard and mouse boundaries; selected Windows CS threaded-camera outcomes | Complete App/System/Surface and font selection/readability, clipboard publication, Mac physical input and quiescence |
| Renderer | Private command/state/texture layer; three selected D3D12 raster profiles and one Metal profile match frozen GL RGB bytes; offline DXBC/metallib tooling | Production wiring, unsupported legacy branches, full world/model/UI/media presentation and parity |

The ABI descriptor does not make every module safe to exchange native C++ objects.
The AppCold loader, clipboard owners and alternate TextEntry lifetime components
remain unselected until their real host ownership and shutdown prerequisites hold.
Font lookup or raster availability alone does not qualify layout or readable UI.

## Remaining work and gates

1. **Close interfaces and serialization.** Finish native C++ module boundaries,
   reject mismatches before callback exchange, and run high-address fixtures.
   Separate native entity offsets from fixed-width network/save identifiers.
   Verify every save callback, transition and C4 phase, including legacy imports
   reconstructed from attenuation. Complete bidirectional demo/save/nav tests.
2. **Complete native platform owners.** Replace remaining Windows engine,
   VGUI1/VGUI2, dedicated/admin and A3D dependencies. Preserve VFS search order,
   game/fallback paths, case behavior, settings, directories, clipboard and shell
   functions. Bind real font, input and clipboard services with explicit UTF-16
   boundaries and owner lifetimes. Join workers before releasing their code.
3. **Qualify providers in the game.** Preserve Steam sessions/authentication,
   callbacks, server functions and encoded voice. CEF must retain off-screen
   pixels, input, cookies/cache and MOTD. Keep writable data in user locations.
   Missing voice, authentication stubs or text-only MOTD do not satisfy this gate.
4. **Finish the shared renderer.** Retain Windows OpenGL for comparison; implement
   the shared command/state contract through SDL_GPU D3D12/Metal, with offline
   shaders. Cover fixed-function state/topology, world/models/sprites, lightmaps,
   fog/transparency, decals/particles/triangle APIs, VGUI/browser/video textures,
   screenshots, MSAA and display controls. Select SDL_GPU only after parity.
5. **Accept mandatory targets.** Run complete Windows x64 first, then Mac ARM64,
   including local app-bundle helpers/library paths and writable-data separation.
   Require Release and Debug evidence below before promotion.
6. **Evaluate conditional targets.** Check actual ARM64 images/native execution and
   complete provider/gameplay behavior. Linux may add small build/naming/package
   changes using POSIX, SDL and existing providers; defer either architecture if
   a new backend/provider or substantial platform work is necessary.

All 19 runtime responsibilities remain in the acceptance inventory. Keep libraries,
PDBs, import archives, tests and intermediates outside runtime outputs; Release
symbols and distribution materials remain opt-in. After mandatory acceptance,
make Windows x64 the default while preserving the root runtime-output contract
and isolating explicit Win32 reference outputs.

## Promotion evidence

For each target, keep compilation, static inspection, module loading, gameplay and
external-server evidence separate. Require Release and Debug checks for:

- Actual binary architecture, exports/imports/resources, module mismatch rejection
  and strings/handles/allocations above 4 GiB.
- Win32 ↔ Windows x64 ↔ Mac demos, saves and navigation, including entity handles,
  symbolic callbacks, transitions and every C4 phase.
- Seeded movement, prediction, collision, RNG, weapons, bots, bomb/defuse and HL
  entities. Compare exact encoded bytes and gameplay decisions; no blanket float
  tolerance may conceal differences.
- Renderer scenes, mixer PCM, capture/microphone/voice, Unicode/IME, cursor/focus,
  DPI/controllers, browser behavior and repeated startup/shutdown.
- Listen/dedicated servers and HLTV; reconnect/changelevel, downloads, consistency,
  AMXX menus/messages/cvar queries and voice against the unchanged Win32 fixture.
  Add actual user server configurations when supplied.

Use isolated, authorized runtime/provider/network fixtures. A buildable target,
a runnable target and a qualified target are distinct states.
