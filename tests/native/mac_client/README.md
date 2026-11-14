# Actual macOS client libraries

Build the [Apple Silicon client](../../../README.md#macos-apple-silicon)
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

The surface-selection case loads distinct real engine and standalone Surface026
providers, checks the standalone/filesystem ABI and complete interface views, and
calls the existing `hw.dylib` controls initializer. Both the engine controls accessor
and VGUI's internal consumer must select the engine-first factory, select the
standalone provider when order is reversed, and select the engine again after
shutdown/reinitialization. Discovery of standalone interfaces starts no SDL video
or audio; loading the hardware engine retains its existing default-cursor video
initialization, while consumer selection creates no windows or extra subsystems.
This uses the compiled consumers and image-owned views, without substituting surface
pointers or copying selection code. It does not execute `CBaseUI::Start` or render
the full client; those remain separate live-game checks.

The image case checks ARM64 architecture and eagerly loads all 13 client dylibs in
separate processes, with the runtime as working directory for provider lookup. The
launcher is checked as the fourteenth production responsibility. CEF runtime behavior
has its own fixtures; this case does not start Steam sessions or launch either game.

Passing these cases does not establish physical input/IME, host General clipboard
publication, Windows text-allocation padding, default font metric/layout parity,
standalone-window event routing, embedded VGUI root lifetimes, renderer scene parity
or gameplay acceptance.

The additional cases resolve native filesystem/GameUI names through the actual
VFS, open/close the real engine's anchored root binding, reject missing/mixed
engine/VGUI cohorts through the finished launcher, and exercise public SDK
TextImage virtual overloads plus a TextGrid allocation canary. These catch
consumer/provider contract differences that provider-only UI tests cannot see.
They do not establish full game acceptance.
