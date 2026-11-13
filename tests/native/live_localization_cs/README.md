# CS live localization native checks

Run from the repository root:

```sh
python3 tests/native/live_localization_cs/run.py
python3 tests/native/live_localization_cs/compile_production.py
```

`run.py` compiles ten ASan/UBSan binaries. The shared snapshot test includes the real header; the remaining fixtures extract the actual production owning methods without rewriting their bodies, providing controlled catalogue/font/UI dependencies. They check:

- borrowed text lifetime, explicit catalogue/literal provenance, copy/move and style/timing preservation;
- active HUD hint ownership, preserved deadline/style and literal network `#` payload;
- center text translation without renewed expiry/position or translated player names;
- visible structured chat/radio templates with original names/body, unchanged scroll timer/flags, and preserved plain history;
- completed legacy menus versus incomplete packet assembly, unchanged valid slots/deadline;
- status text translation without changing pointed player, health or health-display expiry;
- bounded title expansion when a translation exceeds the caller's buffer;
- nested command menu text refresh preserving objects, actions, active selection and popup visibility.

The bounded text fixture first reproduced an ASan stack overflow in the previous production implementation. Snapshot/menu/chat additions were preceded by missing-feature checks. These tests do not write user settings and do not send gameplay/network commands.

`compile_production.py` reuses the existing macOS compile database and warning flags for changed CS translation units, removes object output/compile actions, and adds `-fsyntax-only`. `--database` accepts another existing compile database. It never configures/builds or invokes Ninja. Its TU list/result is written to `build/live-localization/cs-syntax-results.txt`.

These fixtures and syntax checks do not establish live game acceptance. Engine/font transaction behavior and interactive match/menu tests remain integration checks.

`viewport_tests.py` executes the actual viewport refresh dispatcher before a map,
in an active session, and after a disconnect. Presentation callbacks still run,
while the gameplay spectator update is forbidden: its settings checks require a
live local entity and can change spectator state. The native main-menu regression
also switches Russian ↔ Polish without loading a map.

`spectator_tests.py` executes the real spectator formatter and replay methods.
It checks unchanged-mode map/count/player captions, owned inputs after their
original buffers are overwritten, cold/post-disconnect replay, literal balance
overrides, quiet combo updates, row identity/selection and destructor cleanup.
