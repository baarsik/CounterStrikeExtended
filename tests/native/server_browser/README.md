# Native server-browser component fixtures

These component fixtures execute the private GameUI model/config implementations. The production 64-bit
GameUI now constructs the native browser through its module factory; Win32 keeps its external browser.
The historical component packet described here does not qualify full menu activation, connection commands,
password dialogs, details queries or the production callback pump.

The November 13 six-finding follow-up restores consistent game-directory case matching, original keyboard
ensure-visible/page navigation, Connect eligibility for offscreen logical selections, and previous-column
secondary sorting. Secondary sort state is session-only; older primary-only configuration remains valid.
That follow-up received static source/diff review only. No fixture, build, compiler probe, UI, game or live
provider operation was run; older packet results below apply to their frozen source inputs.

`ServerBrowserModel` borrows typed Steam services on its constructor thread. The existing engine remains
responsible for callback dispatch. Every request owns a distinct callback object and copied filter storage.
Callbacks queue indexes; `Pump` copies provider rows, updates indexes and refilters in bounded row units.
`TakeUpdates` is the frame API; `OrderedKeys` is an explicit O(n) diagnostic/user-action snapshot.
Nonterminal reentrant filter/sort changes are rejected explicitly to protect active row iterators.
Quick refresh reuses the original request/callback and rejects overlapping rounds. Stop releases the request.
Same-list full refresh keeps prior rows until successful membership reconciliation. Failed refresh retains
partial/prior valid data with a failed status. Selected identity includes both connection and query ports.

`BeginClose(false)` requires keeping the model, GameUI image and borrowed Steam services alive. Retry outside
provider/callback dispatch. Destruction while a provider/callback stack is live is an invariant violation.
The current module owner coordinates private retirement before GameUI unload/Steam client shutdown;
the public void shutdown ABI alone cannot acknowledge deferred retirement. The historical component packet
does not qualify that production host handshake.

Favorites/history writes carry explicit app IDs. Only an accepted connection may invoke history recording.
The adapter confirms additions by typed readback because Valve does not document an integer success sentinel.
New favorites use current Unix time; existing timestamps, unrelated flags and records are preserved. Tests
use controlled services only. The live probe never writes favorites/history.

Config uses existing SDL organization/application names and only the `server-browser` subdirectory with
`SERVERBROWSER` path ID. `settings.cfg` has one `key=percent-escaped-value` per line. Known malformed/duplicate
fields or unreadable input disable save until successful recovery; unknown raw lines remain intact. Writes
reserve a temporary file in the same directory, flush/close it, and atomically replace the destination.
Shared `CONFIG` and `InGameDialogConfig.vdf` are untouched. Temp injected roots keep tests out of user prefs.

Run immutable Release, Debug and ASan+UBSan packets (macOS, compiler concurrency 2):

```sh
python3 tests/native/server_browser/run.py --repo . --packet build/browser-component-final \
  --sdl /absolute/path/to/qualified/libSDL3.0.dylib
```

The packet freezes relevant source and tool files, argv/logs, dependencies, objects and executables. It runs
9 model cases, 4 config cases and 1 actual favorites-adapter case per profile, plus 3 fail-fast lifetime/thread
invariant cases per profile. No video/audio initialization or live Steam operation occurs during tests.

The native owning packet tools reuse exact retained GameUI compiler policies, compile every owning TU for
the initial packet, pin its source/header closure and full-link against explicitly approved SDL/tier0 images.
`finalize_gameui_packet.py` verifies all unchanged predecessor dependencies/objects, recompiles the changed
model TU and full-links the final image. Initial source-absence RED is distinct from behavioral regression
RED and from the preexisting runtime missing-browser failure.

Controller-only live probe, from a Steam-capable working directory with the appropriate app identity:

```sh
/absolute/packet/release/provider_probe /absolute/path/to/libsteam_api.dylib 10 8
/absolute/packet/release/provider_probe /absolute/path/to/libsteam_api.dylib 70 8
```

The probe runs the real model, existing Steam provider binding and callback dispatcher; closes/releases the
model before a short post-release callback window and Steam shutdown. Returned rows prove discovery only,
not UI, connection, authentication or a complete migration. Actual provider execution is a separate gate.

For Windows qualification, controller uses VM1 exclusively. Configure the component CMake project with the
matching native SDL3 import library and put its DLL beside the test executables or on the process PATH:

```powershell
cmake -S tests/native/server_browser -B build/browser-x64-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCSE_SDL3_LIBRARY=C:/qualified/SDL3.lib
cmake --build build/browser-x64-release --parallel 2
ctest --test-dir build/browser-x64-release --output-on-failure -V
```

Repeat Debug under the retained x64 MSVC policy. Build the owning native GameUI target using the repository's
native x64 presets and retained flags. For Win32, run default/runtime-x86-debug GameUI builds and verify the
new native source selection stays excluded and external module behavior is unchanged. Preserve source/argv,
compile/link warnings, objects, PE imports/exports and exact SDL/provider pins; no native runtime qualification
is implied by component results. Windows atomic replace/thread paths still require this controller gate.

The two `address_formatter_{little,big}_tests` executables exercise the actual SDK inline formatter: each
checks 1027 cases (all 256 octet values in each position, boundary IPv4 bytes and port 0/65535, plus distinct
query/connection ports). They report the physical host endian order. The opposite compile-time branch
uses explicitly constructed byte layouts; it does not claim execution on a physical big-endian host.
Both branches pass against the exact upstream baseline and patched active helper. The original RED is
the controller's actual MSVC `/analyze` C6340/C2220 owning compile, not a runtime output difference.
The upstream baseline, exact two-statement patch and active-header hashes are recorded in Steam provenance.

For a changed header, `finalize_gameui_packet.py` accepts repeated `--owner` plus `--changed-header`.
It audits every linked owning object's actual depfile and refuses to link if any affected consumer was
not selected for recompilation. Original compiler flags, unchanged inputs and current predecessor objects
remain verified, including earlier model/config fixes.
