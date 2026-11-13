# Interface feedback validation — 2026-11-13

The user's corrections supersede the original manual-restart default. Changed
language Apply/OK now saves successfully, queues all Options page changes, then
queues the existing `_restart` command. The receiver stays alive until dispatch,
including a pending close or repeated Apply/OK. Failed saves, unchanged values
and Cancel do not restart. All ten notices describe the forced restart.

Both Audio and Voice constructor-own an empty hidden obsolete-credit label.
This consumes the externally installed stock resource's Miles settings without
rendering them, including resource/scheme reload. License/attribution documents
and imported historical catalogue credit tokens remain intact.

108 proper-name amendments across28 maintained sources preserve original game,
product and technology names. Canonical VAC shorthand fits short controls; full
Valve Anti-Cheat remains in longer notices. Localized game aliases were removed
from actual title entries, while generic faction/military text remains localized.
Original upstream hashes remain unchanged and per-entry `cse_amendments` records
identify all modified tokens. Regression data documents the naming policy.

## Verification

- The native actual Options fixture passed all ten bundled languages and three
  game-type tab compositions. Typed command capture verifies exactly one language
  restart after all other page callbacks, successful Apply/OK/automatic choices,
  repeated pending input, pending close, failed-save retry, unchanged selection,
  Cancel, command-line override notice, glyphs and640×480 text/tab/page bounds.
  RED tests observed missing restart and discarded pending close owner before
  their fixes. Final logs/measurements: `build/interface-feedback-197c2c3/interface-final`.
- Fresh actual Audio/Voice control fixtures passed English/Russian/Polish,
  including official resource labels, repeated resource and scheme reload,
  volume/quality apply, microphone test lifecycle and unavailable voice API.
  Both originally visible credits were observed RED before the fix.
  Evidence: `build/interface-feedback-197c2c3/sound-final`.
- All21 Python localization tests passed, including canonical names, negative
  alias/pure-title checks, generic faction preservation, token/argument parity,
  encodings, generation and offline/incremental resource staging.
- Native brand checks compared baseline and amended text in all ten languages:
  all30 affected fixed-control constraints fit, with510 total final measurements.
  Fifteen tokens needed canonical VAC shorthand or shorter surrounding prose.
  Evidence: `build/interface-feedback-197c2c3/brand-fit/REPORT.md`,
  `final-comparison.json`, and the driver/owning-recipe receipts.
- Complete macOS ARM64 Release runtime builds passed, including the final
  source-frozen build. Fresh review finished with no remaining material findings.
  App package verification is recorded separately in the delivery artifacts.

## Boundaries

The Options fixture compiles complete unchanged production dialog methods and
actual language controls; unrelated option pages are controlled doubles. Command
capture validates issuance/order, without restarting the test process. Audio and
Voice fixtures compile actual production pages with controlled typed engine APIs.
Private registry and strict-reader providers avoid personal preferences.

Brand fit uses real providers, fonts, glyphs,640×480 geometry and retained resource
settings. An isolated driver narrows glyph collection to amended token names so
unrelated legacy glyph failures do not prevent focused measurement; all other
control/browser measurement code is retained, with an exact recorded diff. The
Spanish baseline glyph guard and unrelated browser overflows are documented and
are not claimed fixed. Many dynamic titles/selectors lack fixed constraints;
the30 measured controls do not prove every possible game UI layout.

No full gameplay session or Windows runtime/UI validation is claimed.
