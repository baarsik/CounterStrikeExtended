# SDL3 source provider

Pinned SDL 3.4.20 release source:
https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20

Archive: https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
SHA-256: `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`

The archive digest matches the official release asset digest. All 86 bundled
`external/SDL3/include/SDL3/*.h` files match this source archive byte for byte.
The archive contains its upstream zlib license and notices; no source patches
are applied. Shared provider builds use the target compiler, retaining upstream
platform feature defaults. Static libraries, SDL tests/examples and installation
rules are disabled; this does not disable runtime subsystems.

The wrapper supplies target-private Release optimization and Debug optimization/
symbol settings because the runtime clears CMake's configuration defaults for its
manifest-owned targets. These settings do not restore global assertion, CRT or
exception defaults. Release symbols remain opt-in.

Native 64-bit Windows and macOS shared builds additionally compile the owned
`common/sdl3_native_mouse_bridge.c` extension, declared by its adjacent header.
It adds `CseSdl_QueryMouseModeV1` to inspect the provider's actual global mouse
mode without pumping or consuming input. It rejects calls before video readiness,
from another thread, with a mismatched contract or through foreign SDL dispatch.
The extension uses private SDL interfaces and is deliberately restricted to this
pinned version; an SDL update requires separate qualification. Upstream archive
bytes, headers, dynapi and feature defaults remain unchanged. The resulting native
shared library is SDL 3.4.20 plus this CSE extension. Win32 retains its pinned
prebuilt provider. Linux does not select the extension yet.

Native 64-bit macOS builds also compile the owned
`common/sdl3_native_cocoa_mouse.mm` service, with public declarations in
`common/sdl3_native_cocoa_mouse.h` and a private typed AppKit service header. It
adds exactly `CseSdl_CheckCocoaMouseOwnedV1`,
`CseSdl_CheckCocoaRelativeFocusV1` and `CseSdl_ReadCocoaMouseAbsoluteV1`.
These fixed-width C exports validate their contract and arguments, then the
same-image mode bridge, before accessing AppKit. They borrow a genuine current
SDL Cocoa native-window property; they do not validate arbitrary pointers or
retain arguments. Relative focus and absolute position require the corresponding
actual global mode. Only the explicit Ready status succeeds; failed position
reads preserve both outputs.

The Cocoa implementation and ARC ownership reside in the already Objective-C
SDL provider. The engine adapter is pure C++ and imports only this C boundary.
The wrapper enables Objective-C++ for this source, skips SDL's internal PCH, and
applies source-private ARC, NoEH with unwind tables, precise floating point,
disabled contraction and hidden internal symbols. No Objective-C sampler code
is compiled into the engine, and no public SDL header or dynapi entry is added.
Provider lifetime must outlast its consumers and serialized window calls.
Component hidden-window and C++ consumer unload tests do not qualify full engine
unload or physical camera behavior.

`CSE_SDL3_FROM_SOURCE=ON` selects this provider. The pinned Win32 binary remains
the default during reference qualification. Native dependency presets select
the source provider. This is dependency compilation support, not game acceptance.
