# Engine image loader checks

`run.py` extracts the actual hardware/software image loaders, surface image methods,
hardware sky/map consumers, software sky loading/filtering, and the engine bitmap-font stream decoder. It compiles each
renderer against the shared production codec with C++14, warnings as errors,
AddressSanitizer and UndefinedBehaviorSanitizer. Filesystem, hunk, texture upload,
and palette adapters are isolated in the test harness; decoder and loader logic
are production source.

Run from the repository root after building the pinned WebP provider:

```sh
python3 tests/native/engine_images/run.py \
    --evidence build/engine-images-tests \
    --webp-library build/webp-codec-debug/lib/Debug/webp/md/libcse_webp_md.a \
    --webp-include build/webp-codec-debug/dependencies/WebP/source/libwebp-1.6.0/src
```

Adjust the archive/configuration path for your build. Without explicit provider
arguments, the runner uses `pkg-config libwebp`. A host `c++` compiler is required.

The checks exercise replacement priority, explicit WebP names, opaque WebP bytes
under a legacy TGA name, extensionless names, TGA/BMP fallback, alpha/orientation,
capacity and file cleanup, RGB565/RGB555 conversion, sky-face dimensions, complete software sky filtering, map
sprite pixels, and font BGRA/glyph conversion. Truncated and oversized RIFF font
streams are rejected. Each executable's output is retained in the evidence folder.

Software sky checks require exact 256x256 geometry at the consumer boundary while
retaining arbitrary image dimensions through the public `R_LoadTGA` API. They
cover tiny/nonsquare sidecars, original TGA dimensions, WebP bytes under a TGA
filename, default-sky fallback and missing faces. The initial software sky
regression test reproduces the previous 8-byte allocation followed by a
fixed-size filtering read under AddressSanitizer.

Software color checks exercise every 16-bit packed value in both RGB555 and
RGB565 modes, including red/high-bit pixels through actual WebP sky filtering.
Run with `UBSAN_OPTIONS=halt_on_error=1` to make sanitizer reports fail immediately.

The runner also compiles a Metal-enabled hardware fixture. It executes the actual
`R_LoadSkys` and `R_DrawSkyBox` owners with controlled BMP palettes to cover first-map
palette selection, per-face colors, reused palette slots after reload, RGBA replacement,
and palette-allocation failure. The ordinary hardware/OpenGL and software fixtures
remain separate. All three variants run under ASan/UBSan.
