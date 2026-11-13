# Owned native CEF browser host

This standalone target builds `cse_cef_host` behind the versioned browser wire protocol. It uses the pinned CEF 156 SDK,
stable API 15600, and the process sandbox. It does not link the vendor's C++ browser ABI into the engine.

The target validates the API header and Release/Debug provider hashes against
`tools/qualification/fixtures/cef/lock.json`. The CMake selector is `api_version=15600`; C++20 is scoped to this provider
and its helpers. macOS application/helper deployment targets are 14.0; no XIB compilation or Xcode.app is required.
The Windows executable is the vendor bootstrap paired with the owned `RunWinMain` DLL.
The standalone fixture and production runtime share `cmake/browser_host`; its provider flags stay in that subtree.
Complete native Windows builds stage its executable, DLLs, resources, locales and notices beside the engine.
Symbols and import libraries remain in the build directory.

## Build and run

From an x64 MSVC command prompt on Windows, with CMake 3.25+, Ninja and the Windows SDK:

```powershell
cmake -S tests/native/cef_host -B build/cef-host-windows-x64-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64 -DCEF_ROOT="C:/absolute/pinned/cef_windows64"
cmake --build build/cef-host-windows-x64-release --parallel
python tests/native/cef_host/run_protocol_fixture.py `
  --executable build/cef-host-windows-x64-release/host/Release/cse_cef_host.exe `
  --evidence "C:/absolute/owned/cef-host-release-receipt"
```

Use `Debug` and a separate `build/cef-host-windows-x64-debug` directory for the Debug provider. Launch the fixture as a
normal user: elevated CEF 156 uses its upstream de-elevation/relaunch path, which is unsuitable for inherited protocol
pipes. The host does not disable that protection or install hooks.

On Apple Silicon macOS, with Command Line Tools:

```sh
cmake -S tests/native/cef_host -B build/cef-host-macos-arm64-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_TARGET_ARCH=arm64 \
  -DCEF_ROOT=/absolute/pinned/cef_macosarm64
cmake --build build/cef-host-macos-arm64-release --parallel
python3 tests/native/cef_host/run_protocol_fixture.py \
  --executable build/cef-host-macos-arm64-release/host/Release/cse_cef_host.app/Contents/MacOS/cse_cef_host \
  --evidence /absolute/owned/cef-host-release-receipt
```

Use `Debug` and a separate build directory for Debug. Keep the executable and all helpers unchanged throughout a
persistence run. The frontend integration driver is built separately by `tests/native/browser_wire`:

```sh
python3 tests/native/cef_provider/run_fixture.py \
  --executable build/browser-wire-macos-arm64-release/browser_cef_probe \
  --argument=--cse-host=/absolute/build/Release/cse_cef_host.app/Contents/MacOS/cse_cef_host \
  --evidence /absolute/owned/cef-host-controller-receipt
```

The host itself accepts `--cse-browser-host --cse-storage=<absolute owned directory>`. stdout contains only framed
protocol messages; diagnostics go to stderr and the CEF log in the owned directory. The CMake target also copies the
entry-point and provider license notices into the application resources or Windows output directory.

## Ownership and behavior

- The input reader cooperatively polls the inherited pipe. UI mutations run on the CEF UI thread. Navigation decisions
  use `OnBeforeResourceLoad`'s asynchronous continuation; the UI and IO threads stay available for cookie RPCs.
  Every incoming frame, including an allow/deny response, uses the same UI queue to preserve cancellation ordering.
- VGUI may configure a view immediately after browser creation. The host retains those commands until the browser's
  initial document and user-agent override are ready, then replays them in order. Pending startup commands are bounded
  to 1,024 messages and 64 MiB of payload per browser. Removing a creating browser cancels those commands; a pending
  close runs on the following UI task so it cannot reenter browser destruction inside `OnAfterCreated`.
- CEF restarts resource requests after redirects. The host records the request ID in `OnResourceRedirect`, then gates
  the repeated `OnBeforeResourceLoad`. Denials cancel the real request before the destination is contacted, preserving
  Chromium's redirect method/body rules. Decisions are serialized per wire browser ID. Cancellation retains an
  in-flight response slot when necessary because the legacy boolean response has no correlation ID.
- `KeyDown` and `KeyUp` use **Windows virtual-key codes** on both platforms. They do not accept SDL keycodes. The owned
  VGUI consumer adapters must normalize their keys before using this protocol. Legacy modifiers are Alt=1, Control=2,
  Shift=4, Keypad=8. Keypad Enter is VK_RETURN plus Keypad=8. `KeyChar` accepts a Unicode scalar or individual UTF-16 units; macOS translates virtual keys to native events.
- Main views use software OSR BGRA frames and a paint acknowledgement. The host retains the latest frame while an
  acknowledgement is pending. Select-menu popup pixels remain separate. Zero-sized views are hidden, with a nonempty
  internal CEF viewport. The configured dimension limit is 8192; CEF frame rates are 1 through 60.
- Each OSR browser owns a real hidden native dialog parent: a top-level HWND on Windows, or an NSView with an NSWindow
  on macOS. It is passed to `SetAsWindowless` and remains alive through `OnBeforeClose`. A following UI task releases
  it after CEF finishes the callback, so final client references on other threads cannot destroy native UI objects.
- Browser navigation, POST bytes, custom headers, native user-agent override, focus, mouse/keyboard input, scrolling,
  JavaScript, link queries and geometry queries remain inside the owned provider. POST is applied at the request
  callback after `LoadURL`, avoiding CEF's documented `LoadRequest` cross-origin initiator restriction.
  The pending target and intercepted URL use CEF's canonical URL specification so changes in spelling,
  including an omitted trailing slash, do not drop the body.
- JavaScript alert/confirm and before-unload decisions use the legacy callback. Prompt text and file selection use
  CEF's native default dialogs because the legacy public response carries no text or filename. The file-dialog event
  remains a notification. The seven localized error strings populate the corresponding error/retry page UI.
- `window.open` uses real native CEF windows and retains the opener relationship. Children have separate process-local
  ownership IDs; their navigation policy uses the parent wire ID and `_blank` target. Child painting, titles, URL
  changes and closure never masquerade as parent events. Parent removal and host EOF close pending/live descendants,
  and shutdown awaits their `OnBeforeClose` or aborted-creation callbacks.
- Cookie APIs use the CEF cookie store. The host requests persistent sessions and disk cache under the configured
  owned root (`root_cache_path`, with `cache_path` set to its `profile` child). CEF can also maintain its `Default`
  profile and service data under that root. Legacy `Init` cache/cookie filenames are not imported or reused.
- `SetClientBuildID` retains its 64-bit metadata. This checkout has no production caller and the host does not invent
  a Steam header or JavaScript behavior for that value.
- EOF cancels outstanding policy operations, closes all tracked browsers/windows, flushes the cookie store, exits the
  CEF loop, joins the input reader and calls `CefShutdown`. Malformed or unsupported commands fail the provider.
  Closing/retired browser IDs accept validated late paint/dialog replies; unrelated mutations remain errors.
  Cancelled dialog replies retain FIFO slots so an old answer cannot approve a replacement page's dialog.
- DOM geometry is untrusted renderer metadata. Numeric types, finite integral values, viewport limits and safe
  content/scroll extents are checked before conversion or scrollbar arithmetic. Malformed metadata is ignored and
  logged while later valid queries remain usable. Negative RTL scroll offsets remain representable.

macOS encryption uses CEF API 15600's owned Keychain service/account fields. The
host validates its own code identity before inspecting signing information. A valid
Apple-signed host uses a stable Release/Debug service owned by CounterStrikeExtended;
update continuity still requires a consistent signing identity and designated
requirement, and has not been qualified with a distribution certificate here.

Ad-hoc development builds use a service and profile directory derived from their
validated code identity and canonical executable path. Release and Debug are
explicitly separated by the owning target, independently of `NDEBUG`. Rebuilding
or moving an unsigned host creates a fresh encrypted development store instead of
requesting approval to read an older executable's protected key. Existing keys and
profiles remain untouched; development cookies persist for that exact build and
location. No Keychain access lists, system preferences, encryption or sandbox
settings are weakened. The build ad-hoc signs only the owned host and helper bundles;
the pinned vendor framework and signature remain unchanged.

`storage_policy.py` runs actual settings-only probes for identity reuse/change,
relocation, Release/Debug separation, Unicode/space paths and an invalid signature.
It initializes no Chromium browser, creates no profile, and accesses no Keychain.
The host's `--cse-inspect-storage-policy` switch is the read-only probe entry point.

Development fixtures use `--cse-fixture-keychain=<64 lowercase hex characters>` on macOS. The identifier is SHA-256
of the unchanged executable bytes plus the absolute evidence directory; every process lifetime in that test reuses
the identifier. CEF creates a real encrypted item under service `CounterStrikeExtended Browser Fixture <identifier>`
and account `CounterStrikeExtended Fixture`. This avoids opening a protected production key after each unsigned
rebuild. The direct protocol runner enables this by default, verifies the host's public namespace marker, checks
that all owned helpers have exited, then deletes only that exact fixture service/account item. The cleanup result
is recorded in `keychain-cleanup.json`; unsuccessful cleanup fails a passing test and is retained for diagnosis.
No key value is read or logged. `--production-keychain` explicitly opts into the host's normal storage-selection policy;
ordinary game/controller configuration omits the fixture flag and retains the production names above.

## Evidence boundaries and integration

The protocol fixture exercises real native CEF processes: cookie RPC during initial/redirect policy decisions,
allow/deny redirects, POST bytes/headers/user agent, file/data MOTD pages, actual native popup window existence,
opener references, close-by-opener, parent-removal cancellation, StopLoad followed by a stale allow response,
keypad-versus-ordinary Enter DOM location, Command focus loss without a delivered release,
POST URL normalization, multiple browser IDs, EOF during a decision and a
persistent-cookie restart. The separate frontend driver adds BGRA resizing, mouse/input, Unicode plus virtual editing
keys, JavaScript, alert/confirm, links, geometry, cookie/localStorage/cache persistence and clean process teardown.

These are provider and frontend component checks. They do not establish old-provider visual/policy equivalence, full
native game integration or browser-engine texture/scrollbar layout acceptance. Those remain separate runtime/reference
acceptance work. No full game or microphone fixture is run here. Native prompt/file interaction has its own controlled
fixture below; a compile or default-handler return value alone does not qualify it on either platform.

Production source membership to integrate is `cef_host.cpp`, `cef_host_renderer.cpp`, and the matching
`cef_host_win.cpp` or `cef_host_mac.mm`; macOS helper targets additionally compile `cef_host_renderer.cpp` and
`cef_host_helper_mac.mm`. Keep `cef_host.h`, the bundle plists, runtime/helper packaging and BSD notices together.
The production root targets, manifests and generated target definitions are intentionally unchanged by this fixture.

## Controlled native-dialog fixture

`run_native_dialog_fixture.py` serves an owned loopback page and waits for real native dialog interaction. It never
passes prompt text or a filename through a CEF callback. It emits each stage as JSON to stdout and `interaction.json`;
use the CUA native-app controls to operate that stage. All file selection is limited to the generated
`cse-native-dialog-selection.txt` in that evidence directory.

```sh
python3 tests/native/cef_host/run_native_dialog_fixture.py \
  --executable build/cef-host-macos-arm64-release/host/Release/cse_cef_host.app/Contents/MacOS/cse_cef_host \
  --evidence /absolute/owned/cef-host-native-dialog-receipt
```

The same arguments work with the Windows bootstrap executable. Cases run in this order, each in an independent host:

1. Enter the printed `prompt_value` into the native prompt and choose **OK**; JavaScript must receive that exact text.
2. Choose **Cancel** on the next native prompt; JavaScript must receive `null`.
3. Use the native chooser to open only the printed `selection_file`; JavaScript must read its exact name and contents.
4. Choose **Cancel** on the next chooser; the DOM must report its native file-input cancellation event.
5. Observe the prompt, then write `interaction-observed.json` with
   `{"case":"remove-prompt","native_dialog_visible":true}`; the fixture removes its browser while the prompt is open.
6. Observe the chooser, then write the same observation file with `case` set to `eof-file`; the fixture sends host EOF
   while the native chooser is open.

Record the native UI observation with the last two signals. Do not submit a signal before seeing the corresponding
dialog. The fixture requires native DOM results, subsequent JavaScript recovery, clean host exits and no surviving
helpers, and records its isolated encrypted Keychain cleanup. `--case` selects one case for diagnosis. The 180-second
interaction deadline allows controlled UI operation; protocol and shutdown deadlines are unchanged.

The pinned SDK's `SetAsWindowless` documentation explicitly describes parent ownership for dialogs and context menus.
CEF's [default JS dialog parent check](https://github.com/chromiumembedded/cef/blob/8ea0c73b5dbed1389c8dafb911a98c41246bb8cb/libcef/browser/javascript_dialog_manager.cc#L305)
rejects a null OSR parent in its Chrome path. macOS uses a separate
[native NSAlert runner](https://github.com/chromiumembedded/cef/blob/8ea0c73b5dbed1389c8dafb911a98c41246bb8cb/libcef/browser/native/javascript_dialog_runner_mac.mm#L88);
its behavior cannot establish Windows acceptance. Preserve platform-specific native interaction receipts.

## Pinned Windows key-event evidence

CEF revision `8ea0c73b5dbed1389c8dafb911a98c41246bb8cb` converts `CefKeyEvent.native_key_code` directly through
`ui::KeycodeConverter::NativeKeycodeToDomCode` in
[`CefBrowserPlatformDelegateNativeWin::TranslateUiKeyEvent`](https://github.com/chromiumembedded/cef/blob/8ea0c73b5dbed1389c8dafb911a98c41246bb8cb/libcef/browser/native/browser_platform_delegate_native_win.cc#L418).
The corresponding
[Chromium 156.0.8078.88 mapping](https://github.com/chromium/chromium/blob/156.0.8078.88/ui/events/keycodes/dom/dom_code_data.inc#L226)
uses compact Windows scan code `0xE01C` for NumpadEnter and `0x001C` for ordinary Enter. Event type/modifiers carry
release and Alt state separately. The host therefore uses `MapVirtualKeyW(..., MAPVK_VK_TO_VSC_EX)` and the extended
prefix for keypad Enter, without packing a Windows message `lParam`.

The SDK's older `tests/cefclient/browser/osr_window_win.cc` sample still assigns `lParam` to that field. Following that
sample in the owned host produced a real Windows RED result: Enter was delivered with empty `KeyboardEvent.code` and
location 0. The fixture retains its strict `NumpadEnter`/location 3 and `Enter`/location 0 assertions; a source-only
conversion check cannot replace that native DOM result.

`run_early_browser_fixture.py` exercises creation followed by immediate size, position, header and navigation commands,
then removal during asynchronous creation while another browser remains usable. Run it with the same `--executable`
and a fresh `--evidence` directory as the protocol fixture. The fixture uses the same isolated encrypted Keychain
identity and cleanup rules on macOS. Widget ownership is checked separately in `tests/native/motd_lifetime/`.

On macOS, `Host` places each isolated profile and its Chromium log beneath
`~/Library/Caches/CounterStrikeExtended/native-validation/cef-host/`, keyed by
the requested evidence storage path. Evidence files remain in the requested
directory, and the result records actual storage paths. This follows the game's
cache-location policy and avoids requiring helper access to protected Documents
or Desktop folders. A GPU helper was observed blocked opening a fixture's
Documents-based `cef.log`; the same binary rendered immediately with cache
storage. No browser graphics flags, sandbox changes or additional access grants
are required. Helper cleanup matches actual storage command-line switches,
rather than parent fixture arguments that merely mention the same path.

`CSE_CEF_FRAME_TRACE=/absolute/path.csv` enables a bounded, UI-thread callback
order recorder in the owned host. It writes only after CEF shuts down, so logging
does not change startup I/O. Normal hosts create no diagnostic files. This is a
macOS investigation aid, separate from protocol or gameplay qualification.
