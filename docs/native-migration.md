# Native x64 and ARM64 migration

Preserve Counter-Strike 1.6, Half-Life, their UI/media, dedicated servers and HLTV.
Win32 remains the maintained reference and default. **No native target has completed
acceptance.** [Build commands](building.md), [bounded evidence](validation.md) and
[component reproduction](../tests/native/README.md) describe different stages.

## Targets

| Target | Commitment | Status |
| --- | --- | --- |
| Windows 11 x64 | Required; first mandatory native target | Frozen all-19 Release/Debug builds and selected CS/HL/server checks; complete acceptance open |
| macOS 14+ Apple Silicon | Required | Combined all-19 Release/Debug builds and image inventories pass; selected earlier M5 Max OpenGL checks; acceptance open |
| Windows ARM64 | Conditional | Selected native components; complete providers/gameplay unqualified |
| Linux x64 / ARM64 | Opportunistic | Reuse shared POSIX/SDL owners; substantial platform/provider work deferred |

Windows ARM64 requires available hardware or a qualified Parallels graphics path.
A necessary x64 provider helper must use a versioned pointer-free protocol and
retain full behavior and clean failure/shutdown. No extra renderer project or
hardware purchase is required for this conditional target. After mandatory
acceptance, promote Windows x64 while retaining explicit Win32 reference outputs.

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

## Implementation boundaries

All 19 runtime responsibilities remain in scope. Generated manifests own source
selection, OS replacements, exports and per-file FP/EH/RTTI/CRT/diagnostic policy.
Keep symbols, import/static libraries, tests and intermediates outside runtime
outputs; Release diagnostics and distribution materials remain opt-in. Rebuild
all owned modules together after ABI changes. Source-built pinned dependencies,
source provenance and provider locks remain authoritative.

- Module admission checks architecture, compiler/pointer profile, role, consumed
  table signatures and cohort hashes before factories/callbacks. A descriptor
  does not qualify every C++ boundary. Retain high-address tests and explicit
  factory/view/slot admission. Borrowed tables and callback targets remain alive
  through quiescence; join workers and retire Steam outside the loader lock before
  unloading code. Reject partial/mixed cohorts without bypasses.
- Preserve VFS search order, fallback/case/symlink/mount behavior, settings and
  writable-data ownership. Native game loading uses `gamedll_osx`/`gamedll_linux`
  with guarded platform aliases. App launch from arbitrary working directories,
  quoting/exact-token command editing and restart intent must preserve the selected
  renderer and mode. Recoverable Apple UDP reachability errors retain normal
  retry/timeout behavior, packet bytes and authentication.
- Dedicated console and graphical administration are implemented owners, with
  real stdin/stdout, partial-init rollback, repeated-session cleanup, generation
  bound callbacks and terminal restoration. Full current-cohort admin/server
  acceptance remains separate. Software rendering retains the actual 16-bit
  RGB555/565 framebuffer, palette/cache, pitch and snapshot rules; window 32-bit
  presentation does not change it. Dedicated loading stays windowless.
- Shared native standalone windows retain newer geometry across older queued
  resize events. Backing allocation retries on an unchanged frame independently
  of native request completion; one Set/Sync occurs per new request. Resolve
  rejection, timeout and accepted clamps explicitly. Retire routes while retaining
  in-call native owners. Pinned Cocoa `SDL_SyncWindow` may block up to 2.5 seconds;
  controlled software-widget checks do not qualify game pixels or physical input.
- Native Mac App/System, font and clipboard owners use explicit CP1252/UTF-16/text
  boundaries and synchronous copied clipboard storage. Unselected experimental
  AppCold/clipboard/TextEntry paths need their real host prerequisites. Font
  availability does not qualify layout, physical input or standalone routing.
- Control-Space ownership applies only to the current gameplay window; text/UI
  contexts retain layout switching and ordinary SDL key delivery. System capture
  ownership suspends relative mode/grab without repeated warps or stale input;
  reacquisition requires current focus/window generation and gameplay intent.
  Preserve mouse behavior, fullscreen restart and the timer's legacy-font colon.
  Console notifications remain bounded by capacity, columns and first NUL.
- Save archives write canonical 260-byte names, signed32 little-endian nonnegative
  lengths and unchanged payloads. Readers validate one complete 260- or prior
  native 1024-byte layout through exact EOF before output creation, reject
  ambiguity/truncation/traversal and preserve input bytes. Zero-count archives
  require empty remainder. Propagate all I/O failures. Keep entity/callback,
  transition, C4, demo and navigation encoding acceptance separate from player
  marker roundtrips. The multisource repair retains the bounded legacy fallback.

### Audio and external providers

mpg123 music, SDL DMA/capture, retained mixing/DSP, native A3D software, copied-GUID
public device enumeration and typed notification owners are implemented. Preserve
track/loop/position/pause intent on focus changes, failure recovery and shutdown.
A3D software acceptance is distinct from OEM algorithms and actual game routing:
stock A2D uses pitch/direct-channel controls; omitted EQ/reflection records must
not be presented as implemented OEM DSP. Retain geometry/reflection producer data,
full-width identities, scalar layouts and source-specific numerical policies.
A2D remains 22050-Hz stereo S16 with 8192-byte buffer, 4096-byte low-water,
20-ms schedule and at most 4096-frame kernel calls; ResMan retains its separate
44100-Hz/88200-byte domain and 10-ms/100-ms schedules.

Native notifications use copied registrations and same-module retained events,
atomic replacement/removal generation fences, real auto/manual reset waits,
cancellation/close precedence, wrap/multiple-lap half-open crossings and once-only
STOP transitions. Keep stream-submitted and render-consumed cursors distinct from
DAC/audible timing. No legacy HANDLE emulation or new engine consumer is implied.
Enumeration copies bounded UTF-8 names/records; tokens are ephemeral, snapshots
remain live through initialization and hotplug requires a new snapshot. Unknown,
stale or foreign identities fail truthfully. See [actual A3D boundary tests](../tests/native/a3d_module/README.md)
and [notification contract](../tests/native/a3d_notifications/README.md).

Selected Speakers output and DMA/music PCM coexistence are bounded evidence;
RODECaster HAL startup blocking, acoustic output/capture/voice, hardware removal,
complete spatial/OEM parity and cross-CPU conversion differences remain open.
Owned Steam sessions must retain authentication, callbacks, servers and encoded
voice. CEF must retain sandboxed helpers, OSR pixels/input, cookies/cache and MOTD;
text-only MOTD, missing voice or success stubs do not satisfy acceptance.

### Faithful original server browser

Port the original Steam browser interface and behavior in both retained games.
The reference is installed i386 `serverbrowser_osx.dylib`, SHA-256
`ce7386935527cdd49e51137fa35628989203158dcc74a6611ffb250ce323a644`, original
resources and pinned recovered procedures. Static recovery is not runtime fidelity.
Native64 keeps the internal GameUI module and real Steam adapter; Win32 retains
its original external module path and public IVGuiModule ABI.

- Use the ordinary resource-backed VGUI2 frame, tabs above the list, optional
  compact filters below it, bottom actions and status line. Visual order is
  Internet, Favorites, History, Spectate, LAN, Friends; preserve persisted enum
  meanings via explicit mappings. Minimum640x384 and original reference 624×278
  page sizing, anchors and saved/workspace bounds apply. Default placement is
  x 5%, y 40%, width 50%, height 55%, constrained by minimum and workspace.
- Retain original Password/Bots/Secure/Players/Name/GameDesc/Map/Ping/hidden-IPAddr
  columns, History LastPlayed, icons, resizable Name and ListSmall→DefaultSmall
  font fallback. Preserve supported user column settings and stable selection.
- Filters apply immediately: active-mod Game restriction, case-insensitive Map
  prefix, original latency choices All/<50/<100/<150/<250/<350/<600, secure tri-state,
  not-full, human players (`players-bots>0`), no-password and valid-account
  (`m_steamID.GetAccountID()!=0`). AccountID is distinct from VAC/IsValid/fullID.
  Clear invisible custom name/region constraints and normalize unsupported old
  settings rather than silently filtering with hidden values.
- Preserve one stateful Refresh/Stop, page-specific quick refresh/add/current/
  history actions, context menus, Enter/double-click and selection enablement.
  Add Server uses `DialogAddServer.res` plus its programmatic Servers list;
  GameInfo owns real PingServer/PlayerDetails polling/retry. Use genuine
  SteamApps::BIsVACBanned for the distinct ban capability; unavailable capability
  remains a blocker. Retain original child-dialog resources and query-port rules.
- Copy provider results while valid; preserve bounded view/callback retirement,
  stale-generation rejection and hide/reopen versus destruction. Cancel/release
  requests before their callbacks, panels, module or Steam owner can retire.
  Numeric validated connect endpoints use connection ports distinct from query
  ports; remote text is display data. History records accepted connections only.
- Mask passwords, set the password cvar directly, queue only validated numeric
  connect commands, clear secrets and restore prior state only if still owned.
  Superseding/retry/disconnect/shutdown must retire pending state without clobbering
  intervening user changes. Preserve favorites/history and unknown config fields.
  Browser preferences use the existing SDL per-user identity and a distinct
  browser-only VFS mount; do not repoint shared CONFIG or write original assets.

Actual acceptance requires both games, all tabs/filter states, resize, selection,
context/secondary dialogs, refresh/stop, close/reopen/focus, controlled discovery,
connect/play/disconnect and authentication. Source/CPU/dummy-widget tests and live
returned rows each cover only their recorded boundaries. No custom replacement UI
or whole-window form scrolling is the fidelity reference.

The user explicitly deferred the missing runtime validations for publication
only. Browser Task 3 and the broader runtime gates remain **INCOMPLETE /
UNQUALIFIED**; publication does not repair the failed receipts or establish full
migration acceptance. The delivered app link and remaining manual-test checklist
are required in the publication handoff. See [validation status](validation.md).

### Shared renderer: remaining production work

OpenGL remains the production/reference backend. Current ordered-frame/semantic
capture and selected offscreen D3D12/Metal pixels do not select a production GPU
renderer. Continue the following gates in Windows x64 Debug/Release, then Mac
ARM64 Debug/Release, with unchanged Win32 OpenGL reference:

1. Bind the semantic dispatch and persistent main-window session; admit ordered,
   immutable segments owning draws/clears/targets/resources. Normal frames submit
   once at explicit end; initialization, browser and video use the same lifetime.
   No callback buffer survives borrowed, and no client DLL owns recorder state.
2. Synchronous ReadPixels executes its exact prefix, applies format/pack rules
   and fills the caller before return; failure leaves its destination unchanged
   and terminates the legacy call. Finish waits all preceding work without a
   readback or implicit frame end. Repeated BeginRendering is view continuation:
   six envmap reads complete before one explicit final end, with no callback replay.
3. Preserve separate logical Front/Back colors, shared default depth/stencil and
   independent FBO draw/read bindings. Reads never substitute latest-scene data.
   Publish dirty Front at explicit barriers without a logical swap; NULL acquire
   retains target/dirty state and valid completion. Swap colors only after actual
   presentation submission; preserve FBO/depth/MSAA contents across prefix reads.
4. Admit each segment before execution. A later error cannot roll back earlier
   readbacks/publications: record partial progress, poison the session, refuse new
   draws/presentation, drain submitted resources and discard only unsubmitted work.
   Keep cumulative budgets and strictly increasing nonzero request IDs across
   segment/seal/abort; no hidden GL fallback, replay or one-frame backend mixing.
5. Match matrices, clip-depth/orientation, projective coordinates, all used
   topology/provoking rules, raster/alpha/depth/blend/cull/fog/scissor/mask/offset,
   palette/precision/pack/subimage/mip/sampler/rectangle/border/anisotropy semantics.
   Unsupported line/point/stencil or driver behavior remains explicit until matched.
   Preserve CPU lighting/mips and source quantization; qualify lowering against GL.
6. Port every world/model/sprite/particle/triangle callback, lightmap/detail/sky/
   water/decal/list path, VGUI/font/browser pixel and startup/WebM frame scope.
   Preserve CPU model arithmetic, order, unload ownership and source resource versions.
   Standalone SDL_Renderer windows retain separate ownership; embedded game UI must
   never resolve that standalone surface or share its window with SDL_GPU.
7. Preserve all eight world shader masks from three texture bits, shader-off paths,
   `gl_use_shaders`/`sv_allow_shaders`, reload/reset and VFS override rejection.
   Original vertex/fragment SHA-256 values are respectively
   `850e07ee3fb2dadc6eec25189538c5431c0fd8c5aaff3b520c1705834ef0b83b` and
   `cdd87d3bb499b3a133cee3cbe1de573354cf9c3a49138c95f8102041b4f3693d`.
   Their transformed xy sampling has no q divide, lightmap uses 128/192 and ordered
   double product, detail uses ×2, alpha follows albedo/vertex color and fog is RGB-only.
   Preserve operation order and record the actual selected VFS provider/hash.
8. Qualify resolution/fullscreen/minimize/focus/HiDPI/recreation, gamma/vsync/MSAA,
   front/back/FBO screenshots and settings/restart. Keep bounded fence lifetimes,
   device/window ownership and backpressure. Use pinned offline DXBC/metallib
   containers/layout/toolchain receipts per backend/configuration, no runtime compiler.
   Retain FXC SDK 10.0.26100.0 and native Metal 27B47/MSL3.1/macOS 14 pins and
   original GL/provider/source policies; toolchain upgrades require separate scope.
9. Run a fresh all-19 combined cohort and full CS/HL scenario union before backend
   promotion. Measure warmed compiler-idle same-hardware pacing; batch only adjacent
   compatible packets without crossing ordering/resource/barrier boundaries. No
   per-draw fence/allocation churn, unbounded caches or invented universal FPS target.
   Keep two bounded in-flight slots and fence-tagged monotonic submission serials.
   Keep OpenGL available/default for unqualified configurations and report the
   intentional 100-FPS cap with `fps_override 0` when interpreting frame intervals.

The private [renderer fixtures](../tests/native/render_state/README.md) and
[offline GPU probes](../tests/native/render_gpu/README.md) retain reproduction and
bounded reference failures. Production Metal/D3D12 gates T3–T12 after the present capture
slices remain open; this publication does not close them.

## Remaining work and gates

The combined Mac all-19 Release/Debug build and image inventory pass within
the frozen all19-002 scope. Windows fix7 passes the four-profile owning GameUI
build and 230 ordinary CPU-case slice. Actual combined-cohort runtime checks
remain open. Complete native C++
boundaries, full serialization/entity/campaign roundtrips, provider failure/
authentication/voice, software/dedicated/admin/HLTV, writable-data/bundle signing,
physical input/audio and production renderer acceptance remain open. Preserve
selected historical user acceptance within its original cohorts; do not transfer
it to later combined images. Installers/publication/notarization remain deferred.

### Deferred: microphone consent blocks initial loading

**Status: deferred at the user's request, 2026-11-10; no implementation change.**
On first launch of the macOS Counter-Strike test client, the microphone consent
dialog appears while the game window is black and prevents initial loading from
finishing until the user responds. This was reported with the
`CounterStrikeExtendedMouseCS110` test bundle. Check Half-Life and the stable
application identity when implementing the fix; their exact first-launch behavior
has not been established by this report.

Expected behavior: load the menu without requiring microphone authorization, then
let the user explicitly enable microphone/voice access afterward. Pending or
denied permission must not make ordinary startup, gameplay or audio output depend
on granting microphone access. Preserve the user's OS permission choice.

The current SDL capture initialization opens and resumes a recording stream in
`GoldSrc-SDK/engine/voice_record_sdl.cpp` before `RecordStart()`. Investigate
decoupling capture-device/permission acquisition from startup. The exact blocking
call has not been profiled; this source path is a follow-up lead, not a completed
root-cause diagnosis. Later acceptance should cover first launch, pending/denied
permission, enabling voice from the loaded menu, later authorization and restart.
Track this as an explicit deferred exception in migration reports; granting
consent during a test does not resolve the startup issue.

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
