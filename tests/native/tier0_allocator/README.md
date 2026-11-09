# Retained tier0 allocator fixture

Build with Ninja from an MSVC environment matching `CSE_TARGET_ARCH` (`x86`,
`x64`, or `arm64`). Release and Debug use the complete generated production
`tier0` target, including its allocator and static initialization. Diagnostic
linker symbols remain in the build directory.

```powershell
cmake -S tests/native/tier0_allocator -B build/tier0-allocator-x64 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64
cmake --build build/tier0-allocator-x64 --parallel
ctest --test-dir build/tier0-allocator-x64 --output-on-failure
```

The four cases check actual DLL loading, every small allocation size through
2049 bytes, independent contents and capacity, native alignment, free-list reuse
without neighboring writes, and reallocation across pools and the CRT boundary.
Native cases require real allocation addresses above 4 GiB. Successful process
exit alone does not count: every case must print its completion marker.

`CSE_TIER0_EXTERNAL_MODULE` selects a preserved complete module for negative
comparison. The pre-fix x64 module loads but fails the native allocation
alignment case. Win32 retains its original storage stride. Native storage uses
Windows' allocation alignment so each free-list slot can hold a native pointer.
The ARM64 build retains the existing unsupported GDPERF path; it does not claim
x86 model-specific performance-counter support.

This fixture qualifies an owned allocator boundary, not game startup, module
interface parity, or gameplay.
