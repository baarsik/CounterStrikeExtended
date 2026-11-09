# VGUI1 signed tick scheduling

This LP64 component project compiles the real `utils/vgui/lib-src/vgui/App.cpp`
with its implementation headers. It calls the public `externalTick`,
`addTickSignal`, and interval APIs and observes actual tick callbacks. A second
translation unit includes only the retained public VGUI headers and measures the
public `App` size against the implementation declaration.

`controlled_bindings.cpp` supplies a deterministic platform clock and the
unrelated platform, clipboard, registry, and scheme setup bindings. No scheduler
method, clock conversion, comparison, or addition is copied into the fixture.
No window, font, game, or provider starts.

```sh
cmake -S tests/native/vgui_tick -B build-vgui-tick -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_VGUI_TICK_SANITIZERS=ON
cmake --build build-vgui-tick --parallel
ctest --test-dir build-vgui-tick --output-on-failure
```

The complete suite covers initial deadlines, the default interval, zero and
negative intervals, callback completion time, INT32_MAX/sign transitions,
0xffffffff/full wrap, multiple wraps, INT_MIN/INT_MAX intervals, LONG_MIN/MAX,
and actual public/implementation timing storage agreement. Expected callbacks
retain the legacy signed comparison, including its early-tick quirk near the
signed boundary; they do not introduce a new rollover comparator.

The sanitizers halt on errors. Leak detection is disabled because the retained
VGUI `Dar` container has no destructor; this fixture does not qualify its existing
allocation ownership. `CSE_VGUI_TICK_LEGACY_RED=ON` only permits the original
implicit narrowing diagnostics so the unchanged production scheduler's
behavioral failures can be demonstrated. The final gate keeps it OFF.

This standalone project adds no default runtime target or manifest membership.
