# Restore native Find Servers for the retained games

The user reports Find Servers does nothing in both Counter-Strike and Half-Life.
The menu calls CTaskbar::OnOpenServerBrowser, then ActivateModule("Servers").
The existing platform VDF requests servers/serverbrowser.dll and
VGuiModuleServerBrowser001. The non-Windows loader rewrites the filename to a
Linux .so, and there is no retained native server-browser implementation. The
old HL VGUI1 browser is not this module and cannot provide the full modern
browser behavior by renaming a library.

The native Steam backend is usable. A diagnostic ARM64 executable, built with
unmodified Valve headers pinned to the same source-sdk-2013 commit as the current
Steam headers, opens the actual current provider and obtains
SteamMatchMakingServers002. In eight-second requests it lists10000 CS servers
with650responses and1541 HL servers with657responses. Both requests are canceled
and released before normal Steam shutdown/OSexit0. These are capability results,
not a browser UI, connection, authentication or full-discovery completeness gate.
Sources, commands, initial loader failure, successful dependency binding and
pins are in build/macos-runtime-bringup-20261109/server-browser-investigation-v1/.
Official API/lifetime reference:
https://partner.steamgames.com/doc/api/ISteamMatchmakingServers?l=english .

## Chosen approach and constraints

Implement a native Servers IVGuiModule within the existing GameUI image on
selected native64 targets. Register a private static interface factory for that
record in the existing module loader; retain its normal initialize/postinitialize,
parent/activate/deactivate/run-frame/shutdown order. Store factory/ownership
explicitly so internal records do not call Sys_GetFactory/UnloadModule with a
fabricated library handle. Win32 retains its current external module behavior.
The existing19 retained runtime responsibilities and public IVGuiModule ABI stay
unchanged. The controller chooses this approach to avoid an extra distributable
module or a second embedded browser UI path. Exact private factory/record shapes
and native target selection require a concrete builder proposal before editing.

Alternatives: a new external native serverbrowser library fits the legacy loader
but adds a deployment/ABI/target responsibility without independent ownership
benefit. Redirecting the menu to the old VGUI1 browser would omit required list,
filter/favorites/history/details behavior. Opening Steam's external UI would not
restore the in-game feature. The user has delegated necessary migration choices
and opted out of questions; no additional design approval pause applies.

Use existing VGUI2 Frame/PropertySheet/ListPanel controls and installed
platform/servers resources/localization. Preserve source assets read-only; add
only missing project-owned resources if demonstrated necessary. A typed Steam
adapter uses the exact pinned isteammatchmaking.h/matchmakingtypes.h, adds their
provenance and validates their ABI against the actual provider. Do not guess a
vtable or borrow a foreign architecture's binary. No network-parser substitute
or global setting, permission, firewall or routing change is needed. Do not
change the microphone initialization/consent issue explicitly deferred by user.

## Required behavior

Find Servers opens an operable, resizable browser in both games, including while
in a match, with ordinary focus/cursor/menu behavior. Match the existing browser
resources, localized labels and retained tabs: Internet, Favorites, History,
LAN, Friends and Spectate. Use the appropriate Steam request rather than a stub.
The current game's app/mod is the default filter; no unsupported game launch
flow is introduced. If selecting another retained game requires launch behavior
not present in the engine, expose browsing/current-game connection truthfully
and bring the concrete compatibility boundary to the controller before editing.

Support refresh/stop, selected/all quick refresh, stable selection, sortable
password/security/name/game/map/player/ping/address fields, keyboard/row-double-click
connect and context actions. Filters include game/mod, map/name, nonempty/not-full,
password, secure status, latency and location where the retained resource offers
them. Apply provider-side supported filters plus equivalent local filters without
claiming a partially refreshed server's defaults are real data. Keep valid results
visible during refresh and show progress, empty/unavailable/error states honestly.
The builder settles exact update batching so10000entries cannot stall gameplay;
budget bounded row work per frame and avoid rebuilding the entire list per response.
Do not silently discard valid provider results to meet a hidden UI cap.

Favorites support add by validated address, add/remove selected and refresh, via
Steam matchmaking's favorite/history semantics. History records actual accepted
connections from the existing ConnectedToGame message (CL_CheckClientState sends
it after ca_active/SIGNONS), never mere selection. Match the pending connection
by numeric address/connection port and retain its distinct query port. Do not
overwrite existing user entries.

The browser task owns a narrow writable-config prerequisite: resolve
SDL_GetPrefPath with the existing native preference organization/application
names (CounterStrikeExtended, Release CounterStrikeExtended or Debug
CounterStrikeExtended Debug), then a server-browser subdirectory. Mount it under
a distinct browser-only VFS path ID and preserve unknown fields/error-safe writes.
Do not repoint the existing shared CONFIG mount: it currently maps relative
platform/config and has no per-user guarantee. Do not migrate or rewrite its
unrelated InGameDialogConfig. Test the path owner through injected temporary
roots; controller preserves/restores any preexisting browser-only config before
live tests. A write failure leaves browsing usable with an honest unsaved-state
status. Full runtime writable-data packaging remains separate; no original Steam
asset or app-bundle write is allowed for new browser persistence.

Details show server information and actual players/rules through typed direct
queries, with their own cancel/complete lifetime. There is no reusable retained
server-password prompt. Implement a browser-owned masked TextEntry dialog using
DialogServerPassword.res, with explicit connect/cancel/retry lifecycle. Set the
existing engine password cvar through Cvar_Set directly, never by command-string
interpolation, and queue only a validated numeric-address connect command. Erase
private entry buffers on terminal/cancel paths; never log or persist the secret.
Keep an owned password value until the existing ConnectedToGame or
DisconnectedFromGame message, then clear/restore the previous cvar only if it
still equals this request's value (preserve an intervening user change). A bad
password retains the existing engine error display; retry is an explicit user
connect action, not an automatic loop. Superseding connect/restart/shutdown
must retire prior pending state. Construct connect commands solely from parsed
numeric address/port and use the correct connection port separately from the
query port. Remote names/map/tags/
rules are display data, never commands or format strings. Validate bounds,
termination, control characters, port/endian conversions and failed responses.

## Ownership and integration

Every request and callback has a single main-thread owner and generation. Copy
provider-owned result data while valid; retain no borrowed server/item pointer.
Refresh/stop/tab change/close/reopen/failed init/restart/shutdown invalidate stale
updates, cancel requests and release handles before their callback object/panel/
GameUI image or Steam session can be retired. Account for synchronous/reentrant
callbacks and callbacks arriving after cancellation while the owner is unwinding;
prove actual provider guarantees rather than assuming cancel is a worker join.
Do not add a callback pump or thread if the existing engine owns RunCallbacks.
No callback may dereference a deleted VGUI panel or call unloaded code. Deactivate
and destruction are distinct: hide/reopen semantics preserve usable state;
shutdown must remove tick/message references through the actual VGUI teardown.

No public engine/client ABI or save/protocol/layout changes; preserve current
native mouse/keyboard/screenshot behavior, audio, renderer selection and Windows
x86 reference. Generated target source lists stay synchronized through the
retained manifest/translator. Only one source implementer; root/live apps/VM1
remain controller-owned, never VM2. At most two compiler processes. Do not edit
while screenshot capture or a prior source owner still owns the worktree.

## Acceptance

Actual owner/model tests cover all tab requests, filters/sorts, partial/failing
results, high counts, numeric addresses/ports, malicious/control-character text,
favorites/history preservation, cancellation/reentrancy/stale-generation and
panel/module/Steam teardown. Real VGUI component tests exercise controls and
focus/text. Build actual native Release/Debug GameUI dependencies, coherent
runtime images and retained Windows Win32/x64 policies with immutable evidence.

Controller stages only reviewed complete native browser candidates. Native UI
gates cover both games opening lists, selecting/filtering/sorting, refresh/stop,
LAN/private owned VM1 fixture discovery, details, connect/gameplay/disconnect,
menu/focus/close/reopen/restart and normal audio/hardware renderer. Use controlled
provider fixtures for favorites/history writes, or reversible preserved test-only
entries; do not modify the user's existing live Steam favorites/history during
a capability experiment. External public connection/authentication is a separate
explicit runtime gate; returned server rows alone do not qualify it. Neither a
component nor a native UI-driven result substitutes for unavailable human mouse/
performance acceptance or closes the entire migration.


## Builder check resolution

The fresh independent builder check asked two material questions: ownership of
the missing per-user configuration root and the nonexistent reusable password
prompt. Both are resolved above as controller decisions: a browser-only safe
mount in this change, leaving shared CONFIG/packaging untouched, and a small
browser-owned prompt using existing direct cvar/connect and actual accepted/
disconnected module messages. These are source-backed requirements, not assumed
user choices. Report: server-browser-investigation-v1/builder-questions.md.

## Controller amendment: imported address formatter conformance

The first actual x64Release owning compile under retained /W4 /WX /analyze rejects the pinned Valve matchmakingtypes.h ToString helper: four explicit int octets are passed to unsigned printf conversions. Native strict components and Windows components had passed, which does not qualify the owning analysis policy. Controller admits only a minimal source conformance patch for both endian branches, preserving emitted positive octet/port strings and every ABI declaration/layout/vtable/encoded field. Keep the original pinned header bytes/notices as an explicit upstream baseline, record original and active hashes plus the exact patch/provenance, and distinguish it from the active include. No warning suppression/analysis weakening, macro hiding of the call or guessed declaration fork is permitted. This source-backed amendment replaces the original demand that the active formatter remain byte-unmodified; exact upstream bytes remain retained. All affected header consumers must rebuild and Windows owning gates must pass before Task1 admission. Ruling/task-1-fix-round-2.md supplies exact evidence and scope.
