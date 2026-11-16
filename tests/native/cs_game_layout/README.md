# CS game-menu layout regressions

From the repository root:

```sh
python3 tests/native/cs_game_layout/run.py
python3 tests/native/cs_game_layout/menus.py
python3 tests/native/cs_game_layout/motd_tests.py
python3 tests/native/cs_game_layout/optional_columns_tests.py
```

The fixtures extract complete functions from their production owners and run with
AddressSanitizer/UndefinedBehaviorSanitizer. The scoreboard checks column budgets
with optional health/money, avatars and short headings at compact and reference
densities, and preserves the Original scaling calculation. The menu fixture runs
the team, fighter, buy and scoreboard layout owners, checking containment, action order,
preview aspect ratio, auxiliary columns, repeated layout, deferred resize after
Original replay, pre-Reset signon and overflow mouse input. The MOTD fixture checks
resource ownership, one title, ANSI/wide activation and reversible title decoration.
The optional-column fixture exercises first-population money/health discovery,
dead/live rows, preferences, missing stats and unchanged Original update timing.
External VGUI services are controlled geometry doubles; these tests do not prove
rendering or asset compatibility.

Runtime acceptance also requires the real game assets: inspect team selection,
main buy, weapon details, equipment and a populated scoreboard at 640x480 and
2560x1440. Switch Original → Smoked → Original while each screen is visible;
compare Original with a pre-change build. For a full roster, verify wheel/thumb
scrolling reaches the final rows. Keep temporary in-game probes outside production
sources and restore the preview libraries afterward.

For fighter/MOTD/observer capture logs produced by a temporary preview probe:

```sh
python3 tests/native/cs_game_layout/verify_observer_capture.py /absolute/path/process.log 2560 1440
```

Capture the named phases expected by that verifier: both teams' hover previews,
Original replay and fresh reopening, MOTD OK, collapsed/expanded observer bars,
next-player controls and picture-in-picture. The observer owner also has an
ASan/UBSan fixture in `tests/native/live_localization_cs/run.py` covering screen
resize, density changes, Frame minimum-height clamping and Original restoration.
