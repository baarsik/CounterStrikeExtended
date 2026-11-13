# Live localization engine owners

Run from the repository root on macOS:

```sh
python3 tests/native/live_localization_engine/run.py --evidence build/live-localization/engine-owners
python3 tests/native/interface_preferences/run.py --root . --evidence build/live-localization/preferences
python3 tests/native/localization_text/test_messages.py --root . --evidence build/live-localization/title-parity
python3 tests/native/interface_language/run.py --root . --evidence build/live-localization/startup-selection
```

All C++ executable fixtures use ASan and UBSan. The state fixture executes the
production State controller with controlled preparation/persistence/commit
providers: requests remain queued, superseded requests stay queryable, malformed
choices are rejected, failures preserve revision and active resources, CLI
precedence is retained, automatic resolution and temporary choices work, and
request history is bounded. It never writes user preferences.

The title fixture exercises the production retained-snapshot owner with English
fallback, invalid input rollback, exact pointer classification, snapshot reuse and
old returned pointers surviving another language. The separate bundled title
parity command compares all twenty CS/HL catalogues with the actual legacy parser,
including names, body bytes, display parameters and timing.

The filesystem fixture compiles the actual CMake-owned provider source membership.
It checks read-only staging against an unchanged active graph; directory/HD/pak
priority; discard and English-empty mount groups; unrelated SKIN and GAMECONFIG;
and an already-open unrelated packed-file handle and its byte cursor. A failed
anchor lookup leaves the active graph unchanged. Literal PACK fixtures are owned
by the test directory. No personal runtime files are modified.

Center fixtures compile the actual hardware/software renderer print/replace
bodies with controlled session fields. They verify the replacement preserves
start/expiry timing, rejects mismatched or expired text, preserves a superseding
literal, works during intermission, recomputes line/erase extents, and bounds
truncated old/new UTF8. The font-reset fixture compiles the actual targeted
CFontManager body with the owning CUtlVector: reserved/invalid handles and other
font handles remain untouched.

Native preference tests use a disposable copied-header CFPreferences domain and a
controlled Windows registry API. They never write the production settings domain.

Production-policy syntax checks are separate:

```sh
python3 tests/native/live_localization_engine/compile_owners.py \
  build/runtime-macos-arm64-release/compile_commands.json \
  build/live-localization/owner-compile
```

The checks do not launch a game or prove physical GPU/input behavior, active-match
UI acceptance, Windows execution, or final app packaging. Root runtime integration
and build validation cover those separate boundaries.
