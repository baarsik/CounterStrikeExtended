# UI WebP loader tests

Build the repository's pinned static WebP provider, then run the adapters:

```sh
cmake -S tests/native/webp_codec -B build/webp-codec-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/webp-codec-debug --parallel
python3 tests/native/webp_ui/run.py
python3 tests/native/webp_ui/run.py --sanitize
```

The runner uses `build/webp-codec-debug` headers and static archive. It never falls
back to a host-installed WebP. `--webp-library` can select another pinned archive.

The 2×2 lossless fixture has distinct colors and alpha on both rows, including
nonzero RGB under a fully transparent pixel. Assertions cover:

- VGUI1 BitmapTGA with unknown stream length, RIFF limits, both alpha conventions,
  FileInputStream replacement/fallback, and the client COM filesystem adapter.
- Both font-atlas decoders' top-down BGRA contract and truncated WebP rejection.
- SchemeManager bitmap-font filename dispatch, including WebP-only atlases,
  preferred sidecars, and missing/invalid replacement fallback to TGA.
- SDL and Win32 surface adapters' replacement/fallback paths, RGBA/BGRA ordering,
  explicit uppercase WebP extension, and the Win32 binary transparency mask.
- GameUI logo decoding, caller capacity, dimensions, and legacy TGA fallback.
- HLTV legacy/explicit banner paths, validated WebP, unchanged opaque download
  bytes, and resource names matching the format.

Bitmap drawing is replaced with a test allocation boundary. VGUI2 fileimage is
copied unchanged into a temporary build directory so filesystem write includes
can be substituted. Other adapters are extracted verbatim from production
functions and use disk-backed test files. Windows GDI allocation is simulated;
these tests do not provide Windows runtime or visual acceptance evidence.

`--baseline-sdl` runs the SDL assertions using the original HEAD implementation,
demonstrating its missing replacement behavior. `--banner-only` narrows a run to
the HLTV adapter; `--font-only` narrows it to SchemeManager atlas lookup.

WebP BitmapTGA images follow the uncompressed TGA alpha convention for the
existing `invertAlpha` flag. The historical RLE TGA branch has a different alpha
interpretation; its existing behavior is retained for TGA files.
