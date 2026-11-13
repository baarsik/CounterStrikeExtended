# Interface feedback corrections

The user explicitly requests forced restart when a language change is applied,
removal of the obsolete Miles voice-page credit, and preservation of original
names for games and recognized technologies throughout the bundled translations.
This overrides the earlier manual-restart design. Existing isolated branch reused.

- [x] Extend actual Options fixture with restart command capture: successful changed
  Apply/OK and automatic reset restart once, unchanged/Cancel/failed saves do not.
  Ensure queued changes on all pages precede restart; retain command-line override.
  Update all ten language notices and current maintenance documentation.
- [x] Remove the loaded MilesVoiceLabel from the Voice page using the existing
  control owner; verify against the actual official resource and preserve other
  voice controls. Current code uses SDL/native voice, with no Miles linkage.
- [x] Review stock and CSE token/legacy translations for game/product/vendor names
  and branded technologies/acronyms. Preserve canonical names while translating
  surrounding prose. Record intentional catalogue amendments/provenance and add
  regression coverage for concrete names, token arguments and language parity.
- [x] Run targeted UI/name/native checks, all ten minimum-geometry pages and owning
  builds; fresh read-only review, commit, then refresh both delivered apps from
  the corrected runtime and verify packages and startup.

Validation and evidence boundaries: [interface feedback validation](../validation/2026-11-13-interface-feedback.md).
