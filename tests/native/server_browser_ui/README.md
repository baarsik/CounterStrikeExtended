# Native server browser UI and lifetime qualification

Task 2 base: `f8d744fc5999a62e5018ad42e65c820ea1694324`. This sibling directory keeps Task 1's
recursive snapshot separate. Its typed Valve declarations and approved active formatter patch are unchanged.

The tests compile production components directly and extract complete actual owner bodies byte-for-byte.
`extract_owners.py` records each source/body hash and original line. It rejects unexpected occurrence counts;
it does not rewrite owner predicates or implement a second retirement state machine. Device, engine service,
Steam and filesystem boundaries below the extracted owners are controlled leaves.

The component suite covers:

- Separate query/action error truth; canonical full-data sort, filter, rank, selection and 10,000-row reachability.
- Aggregate model close/replacement reclamation, coalesced latest intent, cancellation/startup reentry.
- Typed player/rule copy/cancel/complete ownership and bounded callback storage retirement.
- Numeric connection commands, distinct ports, accepted-only history and owned password restoration.
- Scoped preparatory disconnects (including nested listen-server shutdown), terminal failure and supersession.
- Immediate filter membership checks, hidden-selection retirement and selection-preserving wheel scrolling.
- Request-free quick refresh pings after Stop/GameInfo, bounded starts, endpoint validation and cancellation reentry.
- Actual loader selection, failed initialize/postinitialize, module call reentry and once-only retirement.
- Actual VGUI1/VGUI2/Host/Sys/engine refusal chain, typed Apple failure and partial Host video failure.
- Both actual Sys_InitGame, CEngineAPI::Run and RunListenServer bodies: partial startup, retained dependencies,
  bounded-yield retries, restart and fatal invalid-entry/exception controls. Death tests are fixture children,
  not game crashes; the production path uses `std::terminate`, never the fixture's `_Exit` hook.
- Actual Host_Init dedicated nonselection. Apple-only controls use the real Apple compiler branch; no fake
  platform defines are used. The reference loader option requires a genuine 32-bit compiler.

Configure a fresh component packet (at most two compiler processes):

```sh
cmake -S tests/native/server_browser_ui -B /absolute/new-packet -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_BROWSER_UI_SANITIZERS=ON \
  -DCSE_BROWSER_UI_PRESENTATION_TESTS=ON -DCSE_BROWSER_UI_CONNECTION_TESTS=ON \
  -DCSE_BROWSER_UI_DETAILS_TESTS=ON -DCSE_BROWSER_UI_MODEL_RETIREMENT_TESTS=ON
cmake --build /absolute/new-packet --parallel 2
ctest --test-dir /absolute/new-packet --output-on-failure
```

`build_vgui_component.py` freshly compiles actual shared VGUI controls and browser sources under retained
GameUI owning recipes. `--driver module` also compiles the actual internal module and accepted GameUI hooks;
`--sanitizers` instruments this component. The admitted predecessor VGUI/filesystem/font images must first
pass `audit_vgui_predecessor.py` against their actual dependency/source closure. These dependency images
are not sanitizer-instrumented. Tests use the dummy SDL video driver, original read-only resources, an
explicit temporary preferences path and typed controlled providers; no live Steam favorite/history writes.

Real VGUI cases exercise all six tabs, filters, sorted displayed slots, 10,000 server rows, 20,000 player/rule
results, logical scrollbar endpoints, queued keyboard/double-click actions, password masking/clear, localized
fallback preservation, settings-save failure, and aggregate detail queue/display retirement. Actual queued
panel deletion is observed with serial handles. Extracted GameUI/console/BaseUI/surface owners use real panels
and controlled service leaves to verify reparented-console/root deletion, partial entry and repeat retirement.
These state controls do not pretend to run a whole game or every void initializer.

`build_gameui_packet.py` compiles every retained GameUI TU plus all new sources and full-links with pinned
SDL/tier0 images. Engine qualification uses the existing `gameplay_shortcuts/build_engine_packet.py`, which
checks the complete affected dependency closure and genuine-main predecessor objects. Generated manifests
come from the retained vcxproj/translator, not independent edits.

Historical absence/configure/compiler fixture failures are not behavioral RED. Real RED packets include
retirement/loader/action errors, panel-owner leaks, embedded-root use-after-delete risk, missing VGUI message-map
inheritance, unusable layout, unordered slot display and aggregate detail reclamation. Qualification receipts
and exact current results belong in the Task 2 SDD report and immutable build packets, not this generic guide.
Windows owning policies, staged both-game behavior, authentication, physical devices and performance remain
separate controller gates. No native component result alone qualifies deployment or parent migration completion.
