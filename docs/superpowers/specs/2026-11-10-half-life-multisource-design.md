# Half-Life delayed multisource compatibility

The native Half-Life campaign must progress from the resonance cascade into
Unforeseen Consequences. The user reports `MultiSrc:Used by non member
DelayedUse.` followed by a persistent black screen. This repair is authorized
within the ongoing complete native64 migration; implementation choices are
delegated to the controller without another approval pause.

## Evidence and cause

Preserved evidence is in
`build/macos-runtime-bringup-20261109/hl109-unforeseen-consequences-black-screen/`.
The complete log, save/transition directory files and qconsole were copied before any
runtime mutation. Read-only main-frame inspection of PID60177 shows:

- Map `c1a0c`, active/sign-on complete, unpaused, M5 Max GPU.
- Fade-in has expired: client time112.76, fade end6.61, flags0.
- Player remains at the map's dark `info_player_start`, approximately
  `(-2106,894,-1548)`, instead of the destination `(1257,614,-384)`.
- `tele_start_ms` has one registered `trigger_relay`, with its triggered bit0.
- The owned map starts `fade_in`; that relay has delay1 and targets the master
  `tele_start_ms`. Its delayed proxy becomes the caller. The current strict
  member lookup rejects that proxy, leaving the teleport disabled.

Valve's published [CMultiSource::Use](https://github.com/ValveSoftware/halflife/blob/master/dlls/buttons.cpp)
increments the lookup index during comparison and tests `i > m_iTotal` afterward.
For valid positive counts, an unmatched caller therefore toggles the final
registered slot. The supplied map relies on this behavior. The original loop
also has unsafe zero-count behavior; reproducing that invalid access is not part
of compatibility. The current repository rejects unmatched callers, including
this legitimate delayed map input, rather than preserving the valid-count result.

## Approaches and controller decision

Preserving an original caller through every delayed entity would add entity
state and persistence/lifetime rules. A bounded implementation of the established
lookup result changes only Half-Life's existing `CMultiSource::Use` body and
preserves saved entity layouts. Choose the bounded compatibility behavior.
Do not modify maps, suppress the warning alone, force teleport state, skip the
chapter, or change fade/rendering code.

## Required behavior

For `1 <= m_iTotal <= MS_MAX_TARGETS`, toggle the first matching registered slot;
if no slot matches, toggle the final registered slot, preserving the legacy
result for delayed, ordinary unmatched and null callers. Keep existing
IsTriggered/global-state/USE_TOGGLE or USE_ON target dispatch semantics.
For counts outside that range, perform no array access, trigger change or target
dispatch. Use the existing array bound. Explain the compatibility fallback in
the source with the affected chapter/map as evidence.

Production scope is only `GoldSrc-SDK/dlls/buttons.cpp`, plus focused actual-source
tests under `tests/native/multisource/`. No class layout, saved field, callback
catalog, public ABI, protocol, asset, Counter-Strike game logic, engine, mouse,
music, renderer, permission or microphone change. The microphone consent issue
remains explicitly deferred. Preserve the accepted CS110 binaries and all saves.

## Validation

Preserve actual-current-source RED for one registered relay and a DelayedUse
caller. Execute the actual Use and IsTriggered bodies with named engine/target
seams, covering every member position, unmatched/null fallback, full capacity,
invalid counts, repeated toggles, initialization and global-state gating.
Run Release/Debug and relevant sanitizers, then compile/link native Half-Life
servers with retained owning policies and unchanged dependency/object pins.
Controller owns Windows owning compilation, live staging and physical gameplay.
The actual runtime gate starts `c1a0c`, observes normal fade completion and player
teleport into the chapter, then exercises the affected transition/save flow.
No source/component result alone closes gameplay acceptance or the migration.
