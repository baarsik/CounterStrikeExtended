# Native engine browser factory fixture

The native engine creates its owned controller in `CBaseUI::Initialize` in
`GoldSrc-SDK/engine/vgui2/BaseUI_Interface.cpp`. It exposes the controller to `BaseUISurface` only after the provider's
versioned handshake succeeds. Native builds do not load `chromehtml.dll` or query its C++ interface factory. The Win32
loader and its `Init("htmlcache", "htmlcookies")` path remain in their existing branch.

`controller_factory.cpp` requires two explicit build definitions:

- `CSE_BROWSER_HOST_RELATIVE_PATH`: UTF-8 path from **the actual `SDL_GetBasePath()` directory** to the staged host.
  Windows staging uses `cse_cef_host.exe` beside the launcher, owned DLL and CEF runtime. macOS packaging must provide
  its chosen relative path and matching SDL bundle-base policy. The code assumes neither `Contents/MacOS` nor
  `Contents/Resources`. A trusted `../Frameworks/...` configuration is supported.
- `CSE_BROWSER_DEBUG_PROFILE`: `1` for Debug and `0` otherwise, for example `$<IF:$<CONFIG:Debug>,1,0>` in CMake.
  Repository configuration policy intentionally clears CMake's default `NDEBUG` and Debug flags, so those macros do
  not reliably identify the selected runtime configuration.

The writable root comes from `SDL_GetPrefPath("CounterStrikeExtended", app)`, with app `CounterStrikeExtended` or
`CounterStrikeExtended Debug`. Browser data lives in its `browser` child. This is a fresh owned CEF profile; the legacy
cache/cookie filenames are not imported or reinterpreted as native locations. Existing game configuration and save
files are not moved by this component. Their native writable-location integration remains separate plan work.
Production configuration supplies no fixture Keychain identifier and retains the provider's stable real encryption
identity. Host/profile paths never fall back to the working directory or the install/data directory for writes.

SDL3's base path is cached borrowed `const char*`; the factory does **not** free it. Its preference path is a distinct
owned allocation released with `SDL_free`. See the pinned repository `SDL_filesystem.h` and official documentation for
[SDL_GetBasePath](https://wiki.libsdl.org/SDL3/SDL_GetBasePath) and
[SDL_GetPrefPath](https://wiki.libsdl.org/SDL3/SDL_GetPrefPath).

## Shutdown ownership

`BeginControllerShutdown` is an owned-local helper, only for instances from `CreateController`; it adds no legacy
vtable or process-wire method. It blocks new widget creation, snapshots existing serializers, detaches each callback
before delivering `BrowserClose` and retains the provider/serializers. Reentrant browser removal and repeated calls
are supported. A normal host close and provider failure use the same callback detachment rule.

Both native HTML consumers schedule VGUI deletion in `BrowserClose`. CBaseUI drains that deletion while GameUI, VGUI,
the surface, serializers and provider remain alive, then shuts down/releases the provider before unloading modules.
The surface clears its native controller pointer afterward. The generic HTML consumer also rejects operations when
provider initialization could not supply a serializer. This is explicit failure handling, with an engine diagnostic;
it does not supply a substitute browser.

## Run bounded tests

```sh
cmake -S tests/native/browser_factory -B build/browser-factory-macos-arm64-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_TARGET_ARCH=arm64 \
  -DCSE_BROWSER_SDL_PROVIDER=/absolute/source-built/runtime/libSDL3.0.dylib
cmake --build build/browser-factory-macos-arm64-release --parallel
ctest --test-dir build/browser-factory-macos-arm64-release --output-on-failure
```

Use Debug and a separate build directory for the Debug profile. Windows x64 uses the same entry point, with
`-DCSE_TARGET_ARCH=x64`, its matching `SDL3.dll`, and `-DCSE_BROWSER_SDL_IMPORT_ARCHIVE=/absolute/SDL3.lib`.
Sanitizer fixtures additionally set `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"` and
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`.
The available AppleClang runtime rejects `ASAN_OPTIONS=detect_leaks=1` before test execution; retained sanitizer
receipts cover AddressSanitizer/UndefinedBehaviorSanitizer, with no LeakSanitizer claim.

The fixture uses the actual pinned SDL process/path APIs and an explicitly controlled framed-protocol host. It checks
Unicode/space-containing host/profile paths, configured relative layout, separate writable roots, actual SDL path
allocation lifetime, missing hosts, profile collisions, handshake rejection/recovery, initialized-only exposure and
deferred widget/provider/module teardown. SDL's real preference-path query can create its application directory;
the fixture never starts a browser there or writes browser cache/Keychain data. Process/profile test data stay under
the selected build directory.

The controller model fixture in `tests/native/browser_wire` adds multiple deferred frames, reentrant removal, repeated
shutdown, ordinary duplicate close events and an actual controlled provider failure during the drain. Run its
`native-browser-controller` test in Release, Debug and sanitizers with matching source-built SDL providers.

These tests do not execute CBaseUI in a full game, qualify texture/UI presentation or replace real CEF acceptance.
Windows native prompt/file interaction remains open because of the recorded CUA guest-input problem. Earlier native
CEF receipts remain tied to their frozen sources and images.

## Runtime integration

The root manifests/translator must select native-only `browser_controller.cpp`, `process_channel.cpp` and
`controller_factory.cpp` for the engine and provide the two definitions above. Keep the wire/command/controller/factory
headers and owned HTML interfaces in that source closure. The engine needs SDL3 process/path APIs but no CEF C++ ABI.
Stage the separately built pinned `cse_cef_host` provider, its matching Release/Debug runtime, helpers, resources and
notices at the configured path. macOS host/helper targets remain separate C++20 provider targets with sandboxing.
Root manifests, generated targets, CPP counts and runtime staging are deliberately untouched by this fixture.
