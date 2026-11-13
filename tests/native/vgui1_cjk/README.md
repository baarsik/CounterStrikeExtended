# VGUI1 actual CJK fallback regression

`run.py` reads compile/link recipes with `ninja -t commands` and directly compiles
fresh actual Font, bitmap platform, font services, native provider, native faces,
raster transfer and CoreText discovery owners with ASan/UBSan. It never runs a
Ninja build. The existing shared registration library is linked unchanged.

```
python3 tests/native/vgui1_cjk/run.py \
  --evidence build/live-localization/vgui1-cjk-green-final \
  --bitmap '/path/to/valve/gfx/vgui/fonts/640_Primary Button Text.tga' \
           '/path/to/valve/gfx/vgui/fonts/1024_Scoreboard Text.tga'
```

The default provider first exercises real system discovery on Arial. SC/TC,
Japanese, Korean, Russian and Polish glyphs must have successful ABC/raster
operations, actual ink distinct from the donor's .notdef, repeatable pixels,
and original cell dimensions/Latin pixels. An observable native-discovery
context proves that 80 repeats perform no extra program reads or scalar
lookups after the first two covering donor fonts. Actual retained bitmap
owners then exercise the Unicode bridge and four release/bind/prepare/preflight
cycles for each supplied font. `--baseline` compiles the provider from Git HEAD
and reproduces the first missing Chinese glyph. Commands, source hashes and
complete stdout/stderr are written under the evidence directory.
