# Browser paint decode benchmark, 21 November 2026

The browser host sends a full BGRA backing image with each paint message. The
game's decoder already owns the complete wire payload when it calls the VGUI
paint callback. The updated decoder passes a view into that payload instead of
allocating and copying another image. The callback consumes the pixels before
the message leaves `RunFrame`; the wire format and callback interface are
unchanged.

Reproduce the decode-only microbenchmark on macOS ARM64 from the repository
root:

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -Werror \
  -Isrc/engine/html/native tools/benchmark_browser_paint_decode.cpp \
  -o /tmp/cse-browser-paint-decode
/tmp/cse-browser-paint-decode
```

The fixture decodes a 1920×1080×4-byte image 300 times per trial. Four paired
trials alternate copy and borrowed-view order. Both paths check sampled pixels
and produce the same checksum. On the Apple M5 Max used for the Metal benchmark:

| Path | Median time per paint | Extra image allocation and copy |
| --- | ---: | ---: |
| Previous `Reader::Bytes` | 0.116508 ms | 8,294,400 bytes |
| Borrowed `Reader::BytesView` | 0.000077 ms | 0 bytes |

The individual copy trials were 0.158026, 0.122647, 0.110369 and 0.108988 ms;
the borrowed trials were 0.000099, 0.000078, 0.000076 and 0.000063 ms. Raw
output is retained locally in `build/metal-cef-20261120/browser-paint-benchmark/results.csv`.
This is a hot-cache decoder measurement. It does not include CEF rendering,
pipe transfer, Metal texture upload or browser frame scheduling, so it does
not establish an end-to-end browser FPS change.

The browser wire tests cover the borrowed pointer, empty and truncated byte
fields. Controller tests exercise full and partial paints, popup pixels and
the synchronous response callback. The callback implementations in both VGUI2
clients upload the texture before returning.
