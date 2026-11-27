# SDL3 source provider

Pinned SDL 3.4.20 release source:
https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20

Archive: https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
SHA-256: `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`

The archive digest matches the official release asset digest. All 86 bundled
`external/SDL3/include/SDL3/*.h` files match this source archive byte for byte.
The archive contains its upstream zlib license and notices. macOS provider builds
apply the narrowly checked `MetalSwapchainPatch.cmake` change to SDL's extracted
Metal backend. It releases the previous swapchain texture before waiting for a
new drawable and returns a null swapchain output when `nextDrawable` is nil;
otherwise the pinned backend submits `presentDrawable:nil`. The archive and
headers remain byte-for-byte upstream. Shared provider builds use the target
compiler, retaining upstream platform feature defaults. Static libraries, SDL tests/examples and installation
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
adds `CseSdl_CheckCocoaMouseOwnedV1`,
`CseSdl_CheckCocoaRelativeFocusV1` and `CseSdl_ReadCocoaMouseAbsoluteV1`.
These fixed-width C exports validate their contract and arguments, then the
same-image mode bridge, before accessing AppKit. They borrow a genuine current
SDL Cocoa native-window property; they do not validate arbitrary pointers or
retain arguments. Relative focus and absolute position require the corresponding
actual global mode. Only the explicit Ready status succeeds; failed position
reads preserve both outputs.

`CseSdl_CheckCocoaInteractionOwnerV1` is an additive mode-independent query using
the same guards and borrowed window lifetime. It reads native active/key/minimized
state, finite positive content geometry, screen-to-content position and the public
`NSWindow windowNumberAtPoint:belowWindowWithWindowNumber:` mouse recipient.
Pass-through windows that ignore mouse events are skipped by that AppKit API.
Ready remains 1; the additive NotInteractionOwner result 8 distinguishes valid
but inactive/foreign/outside interaction from guard or malformed-read failures.
The query does not change cursor presentation, mode or grab, pump/consume input,
warp the pointer, or retain a pointer/callback. Its extra typed private read seams
are separate from the original services and impose no new callback requirements
on existing exports. Actual screenshot-overlay signal qualification is a separate
runtime gate; this unused query alone changes no engine capture behavior.

`CseSdl_SetCocoaGameplayCursorV1` adds an app-local gameplay cursor policy using
AppKit's `NSApplicationPresentationDisableCursorLocationAssistance`. Hidden
gameplay suppresses Shake mouse pointer to locate in both absolute and relative
input modes. Visible UI and explicit retirement restore only the previously
saved assistance bit, preserving current unrelated presentation options. The
engine refreshes the policy after event pumping because fullscreen transitions
can replace the app's presentation mask. Rejected cleanup retains the window for
retry. No system preference changes, input consumption, or mouse algorithm changes
are involved. Scalar window identity and original bit state remain in the provider;
no window pointer or callback is retained. AppKit setter exceptions are contained
at this boundary and nested policy mutation is rejected for retry.

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

`CseSdl_SetCocoaGameplayShortcutsV1` is an additive, initially unused CSE export
with keyboard contract version 1 and the existing Cocoa result values (only
Ready succeeds). It installs one app-local key-down monitor for the borrowed
current SDL Cocoa window. For an active, key, nonminimized current owner, it
filters only native Space with Control and without Command/Option; Shift and
left/right Control remain allowed. SDL3Application dispatches the same event to
SDL before calling NSApplication's dispatch, so the monitor does not inject,
remap or remove SDL events. No text/layout query, mouse mutation, event pump,
global monitor, system preference or permission change is involved.

The provider stores the monitor token and scalar SDL ID/native address/window
number, never a retained window or an engine callback. Main-thread admission and
same-image/video guards precede native mutation. Requests are serialized before
native/property calls; nested matching disable invalidates the generation and
prevents a pending enable from committing. Current SDL property/identity and
generation are checked again before filtering or commit. A different window
cannot silently replace an owner, including reuse of a native address/number
with a different SDL ID. Identical requests do not reinstall the monitor.

Consumers MUST disable successfully before SDL window destruction, SDL shutdown
or provider unload. Disable marks filtering inactive before monitor removal.
Failed removal retains the token and cleanup owner for explicit retry; native
exceptions return Rejected. Removal while a filter is on the stack also returns
Rejected, even when the token is already removed: only a later disable after
callback unwind can return Ready and authorize retirement. All lifecycle calls
and dispatch must remain serialized on the video/main thread. AppKit's monitor
registration must either return its token or fail without registering; no API
can recover a token withheld by an exceptional partial registration. Removal is
considered complete only when AppKit returns normally. The new private typed
seams test the complete actual provider bodies and do not alter older service
completeness or any existing mouse entrypoint.
