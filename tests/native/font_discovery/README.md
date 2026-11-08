# macOS native font discovery fixture

This standalone fixture compiles the production CoreText discovery boundary and,
in a separate executable, the existing FreeType font/manager/amalgam sources.
It does not launch a game, a window, CEF or a provider.

```sh
cmake -S tests/native/font_discovery -B build/font-discovery-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/font-discovery-release
ctest --test-dir build/font-discovery-release --output-on-failure
```

Use `Debug` and another build directory for the debug dependency configuration.
Use `-DCSE_FONT_SANITIZERS=ON` in a separate Debug build for ASan/UBSan.
The dependency loader verifies the source-built ARM64 package inputs, and the
discovery executable checks the linked FreeType version is exactly 2.14.3.
The boundary has no Fontconfig dependency on macOS.

The tests cover family/style and exact PostScript matching, nonzero collection
face indices, long Unicode names, actual FreeType glyph coverage and nonempty
raster output, native CJK/supplementary fallback, malformed UTF-16 and UTF-8,
unknown names, output truncation/reset, repeated creation/cleanup, fallback
cache reuse and discovery/registration lifetimes. Unknown families fail rather
than selecting unrelated fonts. CoreText fallback is restricted to one explicit
UTF-16 scalar; it does not replace the existing layout or shape text.
The native lookup uses Apple's [font descriptor attributes](https://developer.apple.com/documentation/coretext/font-attributes)
and [single-string fallback](https://developer.apple.com/documentation/coretext/ctfontcreateforstring(_:_:_:)).

`make_unicode_fixture.py` changes only name metadata in a local copy of the
installed system Arial font. The generated font remains in the test build
directory, is registered only for the fixture process, and must not be
distributed. No font binary is checked in and no system font is changed.

The fixture does not prove Windows regression, widget/layout equivalence,
pixel/metric parity with GDI, complex-script shaping, color-emoji rendering,
custom-font resource registration by Surface, complete localization encoding,
or game/UI acceptance. Deployment metadata targeting macOS 14 is distinct from
execution on an actual macOS 14 machine.
