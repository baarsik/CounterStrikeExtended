# VGUI1 native allocation and formatting boundary

Both executables compile the actual whole `vgui.cpp`. The default build also
compiles the actual `TaskBar.cpp` to check its bounded clock-text count conversion;
it does not link or execute the widget. No complete VGUI library, game, or UI runs.

The allocator fixture forwards normal requests to real malloc, records the
actual returned native pointer, and checks legal unsized/sized delete callbacks
and reset/restart behavior. On native 64-bit targets, a request above 4 GiB reaches
the recording callback, which throws `std::bad_alloc`; the caller catches it and
checks the full size without allocating oversized storage. Win32's `size_t`
retains the former unsigned-int width and callback ABI. These are callback-boundary
tests, not a large allocation test or general qualification of replacement-new
failure semantics. No production optimization setting is changed.

The formatting fixture captures the actual stdout descriptor in an owned
temporary file. It checks empty output, integer/float/width/precision formatting,
literal percent, UTF-8 bytes, embedded NUL (printed prefix and full return count),
and the old buffer's exact 2,047-byte defined limit. A separate overlong case
checks a 4,096-byte formatted result: output is bounded to 2,047 bytes and the
return remains the full required byte count. The preserved old implementation
passes all defined cases and reports an actual ASan stack overflow on the
overlong case. Only the reference build permits the inherited deprecated
`vsprintf` warning and adds the required delete exception specification; current
production code is compiled with warnings as errors and no warning suppression.

Run from the repository root on Apple Silicon:

```sh
cmake -S tests/native/vgui_allocator -B build/vgui-allocator-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/vgui-allocator-release --parallel
ctest --test-dir build/vgui-allocator-release --output-on-failure
```

Repeat with Debug and a separate build directory. For ASan/UBSan use another
Debug directory and add `-DCSE_VGUI_ALLOCATOR_SANITIZERS=ON`; production `vgui.cpp`
and both fixtures are instrumented. No sanitizer suppressions are applied.

For Windows, run the same configure/build/test sequence from the corresponding
x86, x64, or ARM64 MSVC developer environment; omit Apple's two `CMAKE_OSX_*`
arguments and select a distinct directory. The fixture uses `/W4 /WX`, the proper
Debug/Release DLL CRT, and explicit UTF-8 source encoding. Windows qualification
is separate from local macOS results. Minimum macOS deployment metadata does not
prove execution on the oldest supported macOS release.
