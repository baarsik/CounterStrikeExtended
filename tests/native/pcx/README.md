# PCX writer count and file-format fixture

The fixture compiles the **exact production `pcx_t`, layout assertions and `WritePCXfile` body** extracted from
`GoldSrc-SDK/engine/screen.cpp`. The renderer TU is not linked. `#line` locations and `extracted-bodies.json` identify
each compiled block; the fixture supplies only endian, allocation, memory and synchronous file-output hooks.
No rendering, media, device or graphical API is started.

The serialized header remains 128 bytes with 16-bit fields. Positive dimensions must fit those fields. The writer
checks the legacy `2 * width * height + 1000` worst-case temporary capacity in `size_t` before converting to the hunk's
`int` API. The ceiling also reserves the real hunk header and 16-byte rounding arithmetic. Its final file count is
checked against that already validated capacity before explicit conversion to `COM_WriteFile`'s `int`.

`Hunk_TempAlloc` in `engine/zone.cpp` rounds the request and records a high mark; `Hunk_HighAllocName` rounds again,
adds `sizeof(hunk_t)` and zeroes the complete block. The next temporary/high-mark operation invalidates the allocation.
The writer therefore does not free it, and its reserved PCX header byte remains zero from the hunk. The hook mirrors
that zero-filled payload while surrounding the requested capacity with canaries. Real allocator code is traced and
recorded as a dependency, not replaced in production or claimed as independently qualified by these tests.

```sh
cmake -S tests/native/pcx -B build/pcx-macos-arm64-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_TARGET_ARCH=arm64
cmake --build build/pcx-macos-arm64-release --parallel
ctest --test-dir build/pcx-macos-arm64-release --output-on-failure
```

Use a separate Debug directory/configuration. Add `-DCSE_PCX_SANITIZERS=ON` for AddressSanitizer/UBSan. Windows uses the
same fixture from the appropriate MSVC command prompt with `-DCSE_TARGET_ARCH=x86` or `x64`; no SDL/CEF/provider is
required. Current code compiles with strict conversion diagnostics. The actual Windows matrix is a separate check.

The six cases cover fixed header bytes (including reserved/filler zeros), exact single-pixel escapes for `C0`, `C1`
and `FF`, repeated escape-valued pixels without combining runs, positive padded and negative row strides, all 256 BGR
palette triples and low-byte truncation of word components, dimensions across the signed/unsigned 16-bit boundary,
input/output canaries, allocation failure, nonpositive/unrepresentable dimensions and oversized counts rejected before
allocation or source/file access. A dedicated count pair also fits `int` while overflowing the hunk's rounding/header
capacity; another pair reaches the last valid tall-image allocation request without allocating a multi-gigabyte buffer.
Callers still provide valid source/palette storage for their declared dimensions and row stride.

For a preserved legacy source, set `-DCSE_PCX_SOURCE=/absolute/original/screen.cpp -DCSE_PCX_REFERENCE=ON`. Prefer an
actual Win32 reference build. Run its normal-byte cases with:

```sh
ctest --test-dir build/pcx-reference \
  -R 'pcx-(exact-bytes|row-stride|word-fields|allocation-failure)$' --output-on-failure
```

Reference mode permits only the old implicit count conversion during compilation. Invalid/oversized rejection cases
remain strict tests and are expected to expose the old writer's missing validation; they are not waived as passing.
The original local body passes the four byte/failure cases and has a retained failing large-count receipt. This is
file-format and count evidence, not screenshot-scene, full renderer or playable-target acceptance.
