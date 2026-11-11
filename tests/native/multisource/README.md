# Half-Life multisource compatibility fixture

This fixture extracts the complete current `CMultiSource::Use` and
`CMultiSource::IsTriggered` definitions from `GoldSrc-SDK/dlls/buttons.cpp` at
configure time. It does not carry another implementation of either decision.
The array capacity and initialization flag also come from the actual sources.

The regression catches rejection of the delayed caller before toggling the
registered input. Other assertions catch wrong slot selection, unsafe invalid
counts, lost toggles, initialization/global gating errors, and changed target
dispatch arguments. Counts 1, 2 and 32 cover every member position and both
unmatched and null callers. Invalid counts 0, -1 and 33 must preserve arrays and
canaries, make no array access, query no global state and dispatch no target.

Named seams replace engine-managed string/alert services, live EHANDLE resolution,
global-state lookup and target dispatch. Instrumented fixed arrays observe every
index operation and reject out-of-bounds access. The fixture is behavioral; its
test class is not an ABI/layout substitute. Engine handle expiry, delayed entity
allocation, entity registration, save/restore and physical chapter progression
remain integration responsibilities.

The chapter case uses preserved `c1a0c` evidence: the `multisource` named
`tele_start_ms` has one registered `trigger_relay`, named `fade_in`, with
`delay=1` and `target=tele_start_ms`; its bit starts at zero. `CBaseDelay` creates
`DelayedUse`, and `DelayThink` reaches `FireTargets` with that proxy as caller.
The source decision must set bit 1 and enable the master. These facts are bound
to the controller's preserved packet at
`build/macos-runtime-bringup-20261109/hl109-unforeseen-consequences-black-screen/`,
specifically `c1a0c-teleport-chain.json`, `c1a0c-selected-entities.json` and
`live-teleport-chain-1.json`; no game assets are copied into this test.

Valve's [pinned source implementation](https://github.com/ValveSoftware/halflife/blob/de70fada5061965884492178a7d6166646424a4e/dlls/buttons.cpp)
has the unmatched positive-count final-slot result. Its unsafe zero-count access
is deliberately excluded. This is a source compatibility comparison, not a
shipped binary equivalence claim.

Run from the repository root, placing all outputs in a fresh evidence directory:

```sh
cmake -S tests/native/multisource -B /absolute/evidence/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /absolute/evidence/Release --parallel 2
ctest --test-dir /absolute/evidence/Release --output-on-failure -V
```

Repeat with `-DCMAKE_BUILD_TYPE=Debug`; add
`-DCSE_MULTISOURCE_SANITIZERS=ON` for Debug ASan/UBSan on Clang.
`-DCSE_MULTISOURCE_SOURCE=/absolute/frozen/buttons.cpp` can bind the RED run to
an immutable pre-edit source copy alongside its original `cbase.h`.

The task's immutable RED/GREEN/native packets live outside the checkout under
`~/Library/Caches/CounterStrikeExtended/CodexQualification/hl-multisource-20261110/task-1/`.
They preserve original source before editing, complete command records and source,
test, dependency, object and image hashes. Native images are evidence outputs;
the controller owns Windows compilation, runtime staging and gameplay acceptance.
