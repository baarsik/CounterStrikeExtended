# VGUI1 UTF8 text regression fixture

Imported Russian/Polish strings are UTF8. The retained VGUI1 TextImage/Font loops
previously measured and painted individual bytes; its Apple font/atlas provider
accepted only 256 CP1252 identities. This fixture checks the bounded BMP text
path added without changing public VGUI1 object layouts or virtual slots.

Valid UTF8 runs decode before measuring, word wrapping and drawing. An invalid
run remains entirely legacy bytes, including any valid-looking suffix. Unicode
Latin-1 identities are privately tagged through existing integer glyph methods:
this prevents UTF8 `ó` from becoming ANSI byte `0xf3` (Cyrillic `у` on CP1251).
Supplementary scalars consume their complete UTF8 sequence and display one
replacement glyph; supplementary/grapheme support is outside this change.

The byte font atlases/TGA cells remain intact. Unicode cells use actual cmap
glyphs and are cached by the existing native atlas lifetime. Missing bitmap
cells use a lazy donor from the requested family. Missing Unicode glyphs in
Courier/FixedSys use the existing Arial fallback family; a glyph missing even
there renders the donor's .notdef instead of aborting.

## Checks

- `vgui1-utf8-real-font` compiles source-extracted actual Font::getTextSize and
  TextImage wrapping/paint methods against real NativeFontFace metrics/raster
  from installed Arial. It checks Russian and Polish glyph identities, widths,
  distinct visible raster ink, whole-word wrapping, agreement between extent and
  paint, ASCII CRLF, empty strings, malformed/truncated UTF8 and legacy CP1252.
- `vgui1-utf8-actual-atlas` uses the actual Font owner, Selection, Runtime and
  native Atlas in genuine owned dylibs with a controlled CPU renderer. It
  checks six Cyrillic glyphs instead of twelve bytes, exact ABC endpen, extra
  atlas admission, warm zero-allocation/zero-upload draws, invalid-byte fallback
  and retirement through the existing deferred root-release host.
- `vgui1-utf8-official-bitmap` compiles the actual Font owner/services and loads
  official `640_Primary Button Text.tga`. Discovery is injected with installed
  Arial/Courier bytes; actual production native faces do the rasterization. It
  checks Cyrillic/Polish donor ink, unchanged legacy TGA pixels/height, actual
  FixedSys missing-Cyrillic fallback, preserved FixedSys byte advance, and safe
  missing-glyph handling.
- `vgui1-utf8-windows-glyph-receiver` compiles actual source-extracted Win32
  FontPlat metric/raster methods with a typed GDI receiver. Unicode `ó` must
  choose W metrics/raster with a different width from the original ANSI byte;
  raw `0xf3` must retain A behavior. This is an API-choice test, not a Windows
  GDI execution or a game screenshot.

The initial test failed on the old provider's missing Cyrillic Unicode glyph.
A real Courier donor test also demonstrated GlyphIndex(U+0416)=0, requiring
the existing Arial fallback. The missing-U+FFFF owner test failed by aborting
before the .notdef correction. All these cases now pass.

## Commands used on macOS

From the repository root:

```sh
cmake -S tests/native/vgui1_utf8 -B build/vgui1-utf8 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_FREETYPE_PREFIX=/Users/mmalakhouski/Documents/GitHub/CounterStrikeExtended/build/native-third-party/macos14/cse-arm64-macos \
  -DCSE_HL_GAME='/Users/mmalakhouski/Library/Application Support/Steam/steamapps/common/Half-Life'
cmake --build build/vgui1-utf8 --parallel 2
ctest --test-dir build/vgui1-utf8 --output-on-failure
```

Use `-DCSE_PROVIDER_SANITIZERS=ON` and a separate build directory for ASan/UBSan.
The fixture includes existing native-provider and native-atlas CTest suites;
the complete ordinary run has 17 tests. The focused four tests are selected by
`ctest --test-dir build/vgui1-utf8 -R '^vgui1-utf8-' --output-on-failure`.

No full-game window, physical input, full Win32 build or Windows GDI runtime
was exercised here. Production target compilation and full-game acceptance
remain separate integration checks.
