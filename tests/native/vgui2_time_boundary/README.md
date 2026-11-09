# VGUI2 millisecond declaration boundary

This C++14 standalone suite includes the actual ISystem headers through their two
independent declarations and forwarding entry points. It builds eight contexts:
canonical-first and CS-first include lookup orders, each with the engine header,
normal lookup, standalone wrapper, and explicit CS wrapper.

The actual virtual method's return type must be signed32; Windows must retain
`long`, and non-Windows must use `std::int32_t`. A separately compiled fake producer
uses that actual return type and only valid, representable double inputs to test
16777217 ms, INT32_MAX, and fractional truncation. It supplies no production clock,
virtual provider, out-of-range conversion, rollover, or deadline implementation.

```sh
cmake -S tests/native/vgui2_time_boundary -B build-vgui2-time -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-vgui2-time --parallel
ctest --test-dir build-vgui2-time --output-on-failure
```

The engine and canonical class definitions are separate declaring contexts; this
suite does not combine both definitions into one translation unit. No runtime
target, source manifest, root policy, provider, or game launch is added.
