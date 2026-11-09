# Unselected FixedSys18 role component

This fixture uses authored SFNT geometry and injected lookup/registration callbacks. It does not query system fonts,
execute CoreText discovery/registration, create a window, or select a production provider. The normal CTest suite
contains only the authored/injected case and the three unchanged baseline cases on Apple (two on Windows).

The exact private tag `apple-fixedsys18-courier-cell8x15-gray-v1` admits only requested FixedSys18/400, width/angle/styles
zero, ANSI1252 and explicit Courier lookup identity. The new face validates neutral OS/2-less indexed SFNT metadata,
218 graphic mappings/common positive ASCII design advance, and complete nonempty gray ink for the 216 nonspace graphics
and glyph0. It searches one largest fitting integer ppem <=12, with one shared translation/baseline and no source crop.
All 256 immutable cells and raw ABCs are owned before readiness; logical H15/W8, baseline12/3, leading3 and ABC0/8/0 are
the named role policy. Numeric donor weight remains unknown. Weight capability means only the approved neutral role.

The independently pinned 256-byte route string verifies 16 blank, 22 native-notdef (including explicit08), two authored
6x9 default-block and 216 mapped-graphic cells. Independent FreeType rendering verifies the chosen scale, failure of
every larger scale, actual glyph mapping, shared origin, every cached native cell and the sealed BGR/weighted-alpha
transfer. Tests cover owned input lifetime, call order, nonzero pens, guard bytes, invalid-output atomicity, missing or
empty glyphs, metadata/style/table/identity/fit failures, ordinary-face rejection, injected dispatch, failed-release
retention/retry and repeated valid ownership. The authored donor has variable non-ASCII punctuation advances, an empty
08 mapping and no OS/2 table; it is not a copy of Courier or another OS font.

Build with CMake/Ninja and the pinned FreeType2.14.3 source-built prefix:

```sh
cmake -S tests/native/vgui1_fixedsys_role -B build/vgui1-fixedsys-role-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_FREETYPE_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/vgui1-fixedsys-role-release --parallel 4
ctest --test-dir build/vgui1-fixedsys-role-release --output-on-failure -V
```

The face and baseline raster compile as C++14, strict FP, NoEH. Provider/discovery retain their existing C++17 boundary;
test drivers use exceptions, provider code does not. Allocation exhaustion follows the inherited fatal NoEH policy;
ordinary invalid input/resource/style/fit errors return failure. Sanitizers instrument owned code, not pinned static
FreeType/dependency archives. Registration failure keeps context/tokens and requires explicit retry before unload.

`windows_matrix.py` runs authored/injected CPU contexts with actual pinned MSVC/FreeType, x86/x64/ARM64 Release/Debug.
No GDI/system-font API, desktop, game or provider execution is included. `prepare_macos.py` freezes source and builds a
separate `fixedsys_role_real_probe` only. The executable is NOT a CTest case. Its three future 45-second commands require
separate reviewed authorization; they are restricted to the already-pinned Courier/index0 program and this one tuple,
perform no registration, and save only metrics/glyph IDs/route/alpha facts/digests. `--describe` is value-only.

Production remains unselected: root owns the Apple exact owner-profile entry, source membership, complete use/module
leases, retained-borrower Prepare, bootstrap/catalog/atlas and UI acceptance. Existing ordinary faces/Harmony, owner
files and helpers are unchanged. The legacy baseline fixture's old standalone source list does not include this new
face; this new wrapper explicitly adds it without editing that frozen fixture. No qualified factory is defined.
Authored fit does not establish actual Courier fit or readability; gray donor/notdef appearance and real UI layout/
readability remain open, with no GDI pixel-parity or arbitrary tolerance claim. Existing instrumented repeated-dlopen
limitation belongs to the separate owner packet and is not rerun or promoted by this component suite.
