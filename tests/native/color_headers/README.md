# Color declaration bindings

Compile and runtime checks preserve all three retained include-search contexts: engine,
VGUI2 and Counter-Strike. Their color declarations share four-byte RGBA storage
but have different inline APIs and copy traits. The native `cse_color.h` forwarding
headers preserve that selection without relying on a file spelling that differs
between those contexts. Windows retains its original include directive.

```sh
cmake -S tests/native/color_headers -B build/color-headers -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/color-headers
ctest --test-dir build/color-headers --output-on-failure
```

The two existing inherited-method warnings stay visible under the same specific
exceptions used by the runtime policy. This checks declaration selection and
storage, including byte-aligned placement, copy construction, assignment and self-assignment.
It does not qualify rendering or complete C++ module compatibility.
