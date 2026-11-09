# HLTV initial timestamp expression fixture

This standalone diagnostic extracts the exact assignment and subtraction from immutable V11 `DemoFile.cpp`. It preserves `float startTime`, virtual `double GetTime()`, and the retained hltv_core C++17 `/fp:precise` `/EHsc` `/GR` policy, with `/O2 /MD` Release and `/Od /RTC1 /MDd` Debug. It never changes floating-point controls or clocks outside its own controlled object.

Each binary64 clock is supplied by exact integer bits through a volatile double. The fixture logs the stored binary32 start, four same-tick encoded timestamp bit patterns and an advanced-clock result. Cases cover exactly representable clocks, neighbors rounded upward/downward, binary32 midpoints and the residue observed in the real native HLTV file. Configurations are compared against actual Win32 Release output and an independent binary32 rounding model.

Build in a separately initialized MSVC environment for each CPU; use new directories:

```powershell
cmake -S tests/native/hltv_timestamp -B build/hltv-timestamp-x64-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCSE_HLTV_V11_SOURCE=C:/CI/CSE-Native64-RuntimeV11-20261109/source
cmake --build build/hltv-timestamp-x64-release --parallel 2
build/hltv-timestamp-x64-release/hltv_timestamp.exe
```

Run Win32, x64 and ARM64 Release/Debug. Capture PE machine, compiler command/version, source/extraction hashes and FP controls. Reading the control word uses `_controlfp_s(..., 0, 0)`; sticky exception status may change because real conversions are exercised, while rounding/control bits must remain unchanged.

This isolates the actual writer expressions; it does not claim complete HLTV writer or reader acceptance. The original strict producer gate remains failed until a separately reviewed HLTV validation profile and real replay evidence qualify it. No producer bytes or frozen validators are rewritten.

Microsoft documents source-precision rounding at assignments/casts for [`/fp:precise`](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170) and control-word inspection with [`_controlfp_s`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/controlfp-s?view=msvc-170).
