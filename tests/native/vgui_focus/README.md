# Native VGUI focus and pointer-copy fixtures

This standalone C++14 fixture compiles the unchanged `OnRequestFocus` encoder,
the actual `Panel.h` registration macro, and both complete production message
dispatch methods extracted from the shared and Counter-Strike `Panel.cpp`
implementations. It links the real engine, Counter-Strike, and VGUI2 public
`KeyValues.cpp` implementations and their serialization-buffer dependencies.
The fixture supplies no integer-type aliases to production bodies. The public
KeyValues variant uses the Counter-Strike dispatcher, whose production
declarations also use `unsigned long long` without an engine-only `uint64` alias.

The controlled bindings supply panel creation/parent delivery and the external
`IKeyValues` allocator/symbol service. Rendering, native windows, input, the full
panel object layout, and cross-DLL integration are outside this component test.
No pointer is dereferenced at a synthetic panel address.

The cases check focus identities above 4 GiB, distinct high halves, a low-half
sign bit, null and missing arguments, virtual dispatch to a derived receiver,
one- and two-argument dispatch through both map implementations, member-pointer
map copies, and a real `KeyValues::MakeCopy` followed by destruction of the
original message. Pointer-copy cases cover root, child, and null pointers while
also checking unchanged integer and float payloads. Win32 uses the existing
integer focus encoder and map branch; `HPanel` remains a 32-bit registry ID.

```sh
cmake -S tests/native/vgui_focus -B build-vgui-focus -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-vgui-focus --parallel
ctest --test-dir build-vgui-focus --output-on-failure
```

Run from a matching MSVC x86, x64, or ARM64 environment on Windows. The macOS
fixture uses `-fshort-wchar` for the SDK's Windows `wchar_t` assumption; its local
CRT shims abort on unrelated wide numeric parsing or file-loading heap checks.
Use `-DCSE_VGUI_FOCUS_SANITIZERS=ON` for local ASan/UBSan instrumentation. Existing
SDK warnings in unrelated compiled methods are not suppressed.

`compile_production_context.py` additionally compiles the complete Counter-Strike,
engine, and GameUI `Panel.cpp` translation units using recipes read from an
existing runtime Ninja build. Use a private complete source-tree snapshot for
both `--context-root` and `--candidate-root`; compiler objects, PDBs, logs, and
receipts go only to `--output`. This check retains the actual production headers,
compiler definitions, forced includes, and warning policy:

```powershell
python tests/native/vgui_focus/compile_production_context.py `
  --ninja C:/tools/ninja.exe --recipe-build C:/runtime-source/build/runtime-windows-x64-release `
  --recipe-root C:/runtime-source --context-root C:/isolated-source `
  --candidate-root C:/isolated-source --output C:/isolated-check
```

This directory adds no default runtime output or root CMake membership. Passing
these fixtures does not qualify complete game startup, cross-module ABI, or play.
