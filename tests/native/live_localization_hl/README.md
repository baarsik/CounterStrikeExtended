# Half-Life live localization owner fixtures

Run from the repository root:

```sh
python3 tests/native/live_localization_hl/run.py --evidence build/live-localization/hl-green
```

The script extracts unmodified production function bodies and compiles six
ASan/UBSan executables. External engine title lookup, packet input, font allocation
and VGUI drawing/layout endpoints are controlled fixture boundaries; the ownership,
revision presentation and cache mutation code is production code.

- Message ingestion owns catalogue and literal snapshots; relocalization preserves
  styles, start times, ending state and literal director text.
- Actual Draw effect-2 expiry stays at the original inclusive deadline across
  longer/shorter translations, decoded Unicode/newlines, level-clock adjustment,
  replacement, reset and slot reuse; elapsed scan time remains unchanged.
- ShowMenu retains the last complete source through a new multipart assembly;
  refresh preserves slots, expiry and flags and re-expands current key bindings.
  The actual key expansion owner handles input beyond its former fixed buffer.
- VGUI1 bindings retain literal names/MOTD, reload file text and literal translated
  command labels, preserve scroll, rebind font pointers, reuse bounded locale
  generations, and detach callbacks on destruction/explicit close/reset.
- Center TextMsg retains typed source/parameter provenance, never translates a
  literal parameter merely because it later matches a token, rejects stale center
  provenance, and uses replacement without replaying CenterPrint. UTF8 clipping
  retains complete characters while CP1252 byte semantics are preserved.
- HUD talk refresh reflows only typed TextMsg notifications and preserves literal
  chat, color/name metadata, scroll time and flags without sound/console replay.
  Discarded logical messages do not reappear.

Baseline replay should fail on the original defect for these cases:

```sh
python3 tests/native/live_localization_hl/run.py --baseline --case messages --evidence build/live-localization/hl-message-red
python3 tests/native/live_localization_hl/run.py --baseline --case menu --evidence build/live-localization/hl-menu-red
python3 tests/native/live_localization_hl/run.py --baseline --case center --evidence build/live-localization/hl-center-red
python3 tests/native/live_localization_hl/run.py --baseline --case chat --evidence build/live-localization/hl-chat-red
```

The baseline is `git show 538661e:<owner>`; fixture-only dependency definitions remain
current. These are bounded owner tests, not a full game/session, Windows runtime,
visual acceptance or font rasterization test. Existing `hl_menu_localization` and
`hl_message_utf8` fixtures cover the retained localization/renderer paths separately.
