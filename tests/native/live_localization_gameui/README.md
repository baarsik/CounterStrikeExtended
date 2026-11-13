# GameUI live-localization owner regressions

Run `python3 tests/native/live_localization_gameui/owner_tests.py` from the repository root.
The harness compiles actual production owner methods with controlled service/widget
state, using Clang ASan/UBSan. It does not reconstruct the full VGUI runtime.

- `tasks`: actual task declaration, constructor, accessors, formatter and refresh;
  80 switches between short/CJK values retain completion, partials, parameters,
  weapon normalization and flags; long descriptions and reordered/repeated/bounded
  format parameters are covered.
- `keyboard`: actual action-file parser, current localized-file service double;
  updates descriptions and section heading while retaining primary/alternate
  pending bindings, saved bindings, unbind queue, capture, selection and scroll.
- `browser`: actual browser refresh method with observable child/query state;
  summary/detail/add-dialog presentation updates retain query generation, pending
  callbacks, filters, selection, viewport, typed map and password.

`--baseline` builds the task owner from the pinned pre-localization commit `538661e` and reproduces the stale-cache
failure before the live-refresh change. Production recipes are verified separately;
these fixtures do not prove active-game/runtime acceptance or native service behavior.
