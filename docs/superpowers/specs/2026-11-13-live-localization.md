# Engine-owned live localization

Explicit user request: implement shared engine-level localization for CS and HL,
including menus/HUD, live without restart or lost game session; deliver updated
macOS Apple Silicon builds. This supersedes forced-restart language application.

The engine owns one committed language, preference and monotonic revision. A new
versioned factory C-table exposes read state, queued requests and request results.
Existing public engine/client/VGUI interfaces and layouts remain compatible.
Startup CLI > saved bundled preference > Steam/English precedence remains intact.
Empty preference restores automatic selection. Persistent UI requests save only
after preparation succeeds; temporary API requests support isolated validation.

A request commits at an accepted engine frame boundary: prepare catalogue/title/
localized-mount changes without destroying current state, persist if requested,
replace only managed localized paths in their original priority, update surface
and font language, commit stable-index catalogues and safe titles, publish revision
last. Invalid/missing data or failed persistence retain the previous valid state.
Old title allocations remain safe for legacy raw-pointer users until session end.
Repeated supported-language switches must not accumulate unbounded string/font data.

VGUI2 keeps token identity/provenance and rebuilds text/layout/fonts while leaving
literal user input, selections, widget identity and dialog variables intact. Both
shared controls and CS private copies participate. No destructive full scheme or
viewport restart. Engine localization revision is authoritative across libraries.

CS and HL refresh only presentation on revision: active catalogue-backed HUD text,
menus/commands, status/center text, team presentation, scoreboards, spectator labels
and local map/help text. Retain raw typed sources and owned strings where needed;
server/player names, chat/MOTD/demo literals and team identity remain literal. Keep
map, connection, scores, health/money, timers, menu/scroll/input and spectator mode.
No restart/reconnect/map-change/pause commands as part of language refresh.

Validate with RED/GREEN ownership/state/cache tests, native module builds and actual
runtime switching while an active local game continues. Cover all ten languages,
English fallback, short/long/CJK transitions, repeated switching, failed requests,
widget variables/literals and HUD expiry/state. Source-owned translations and brand
policy stay intact. Package both apps and verify hashes, signatures and startup.
