# Actual macOS client libraries

Build the [Apple Silicon client](../../../docs/building.md#macos-apple-silicon-bring-up)
first, supplying the pinned third-party, Steam and CEF paths. Then run:

```sh
cmake -S tests/native/mac_client -B build/mac-client-ui-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCSE_CLIENT_RUNTIME="$PWD/build/runtime-macos-arm64-release/runtime/Release"
cmake --build build/mac-client-ui-release
ctest --test-dir build/mac-client-ui-release --output-on-failure
```

Repeat with Debug and `build/runtime-macos-arm64-debug/runtime/Debug`. Each fixture
links/loads the actual production libraries; it supplies no fake System, Surface,
font provider or platform callbacks.

The UI case selects SDL dummy video/software rendering so it does not change the
user's desktop clipboard. It exercises bounded clipboard reads, native UTF16 cells,
the VGUI1 CP1252 bridge, printable key/name translation, monotonic clocks, process
arguments, calendar/disk services, a real CoreText/FreeType Helvetica glyph, rectangle
and texture pixels, panel border/inset traversal, one Bitmap shared by two windows,
and balanced Surface window/renderer/video-subsystem shutdown. Font preparation is
explicit; no embedded-runtime admission or default-font gate is bypassed.

The image case checks ARM64 architecture and eagerly loads all 13 client dylibs in
separate processes, with the runtime as working directory for provider lookup. The
launcher is checked as the fourteenth production responsibility. CEF runtime behavior
has its own fixtures; this case does not start Steam sessions or launch either game.

Passing these cases does not establish physical input/IME, host General clipboard
publication, Windows text-allocation padding, default font metric/layout parity,
standalone-window event routing, embedded VGUI root lifetimes, renderer scene parity
or gameplay acceptance.
