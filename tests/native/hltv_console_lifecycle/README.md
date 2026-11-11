# Complete production HLTV Console lifecycle

macOS ARM64 qualification using a configured, built owning runtime and an immutable,
finished owned filesystem provider. The runner copies only HLTV, Core, Proxy, Steam's
link dependency and the filesystem into a new private stage. Every final socket binds
`127.0.0.1` with port `0`; no shared runtime, game data, Steam session or GUI is used.
The empty `valve/` directory is necessary because filesystem Mount selects it as the
first write path. Logs and stages remain available as evidence.

```sh
cmake --build build/demo-interface-runtime-arm64-release-v1 --target hltv hltv_core hltv_proxy --parallel 4
python3 tests/native/hltv_console_lifecycle/run.py \
  --runtime-build build/demo-interface-runtime-arm64-release-v1 \
  --filesystem /absolute/path/to/finished/filesystem_stdio.dylib \
  --output build/console-lifecycle-release
```

Use the matching Debug build/provider for Debug, and add `--sanitize-console` to
instrument the complete Console `System.cpp` translation unit with ASan/UBSan.
Other objects and providers retain their owning build flags in that focused run;
it is not an instrumented full-runtime acceptance claim. Run the existing
`tests/native/system_modules` sanitizer components separately. Its known sanitizer
image reload limitation remains independent of this test.

The nine cases exercise actual command quit/exit, stdin EOF, SIGTERM, repeated
cleanup with a second complete Init, callback-deferred cleanup with recursive
ShutDown, pre-console Init failure, missing required Proxy after filesystem Mount,
and missing filesystem after Console Init. Every successful run must print each of
all seven provider/embedded module teardown messages exactly once. SIGTERM uses
the real filesystem logfile because existing console output intentionally stops
writing after the signal flag is set. The fixture also checks filesystem pointer
retirement, descriptor retirement (descriptors below 256), rejection of Init during
pending retirement, and exactly one borrowed-module cleanup across reentry/retry.

`console_paths.cpp` includes the complete production `System.cpp`, renaming only
its process entry point. The runner derives compile flags from the owning
`compile_commands.json` and relinks the exact owning objects, replacing only the
System object. Init, Stop, ShutDown, admission and provider code are not substituted.
Core/Proxy are real runtime images. Staged binary SHA256 values are printed.

The harness requires the macOS Ninja runtime build layout and intentionally does
not claim Windows execution or full native migration acceptance.

## Optional VGUI acquisition and retirement

`run_vgui.py` uses the complete production owner with narrow controlled VGUI
headers/services and a real loadable `vgui2.dylib` test provider. Core, Proxy,
filesystem and Console/platform objects remain real. The real GUI control
implementations are replaced only in this fixture; no UI is opened and this does
not qualify real VGUI rendering or its ABI.

```sh
python3 tests/native/hltv_console_lifecycle/run_vgui.py \
  --runtime-build build/demo-interface-runtime-arm64-release-v1 \
  --filesystem /absolute/path/to/finished/filesystem_stdio.dylib \
  --output build/console-vgui-release
```

Three cases drive `System::GetPanel` through actual `StartVGUI`: failure loading
the scheme after panel acquisition, successful startup with callback-deferred
owner cleanup, and failed startup followed by successful retry in the same owner.
The controlled panel records construction/destruction, and the loadable provider
records service calls and actual image unload. Assertions require detach before
destruction, destruction before surface shutdown, and no live/embedded panel at
provider unload. Repeated cleanup must do this exactly once per acquisition, and
fully started controls must stop once. Add `--sanitize-console` for the same
focused System TU ASan/UBSan bounds as the baseline harness; VGUI test provider
code and other owning objects retain their normal flags.

## Native Windows provider null-detach path

The owner-service fixture alone cannot validate a surface-provider precondition.
`surface_provider/run.py` therefore extracts the complete production
CWin32Surface setter, CreatePopup, ReleasePanel, SetContextPanel, ContextPlat and
SetPanelVisible methods, plus the actual VPanel IsPopup/Plat/SetPlat/GetParent
methods. Only OS operations and remaining dependency services are controlled; no
real window, desktop or VM is used.

```sh
python3 tests/native/hltv_console_lifecycle/surface_provider/run.py \
  --output build/console-surface-provider-sanitize --profile sanitize
```

Profiles `release`, `debug` and `sanitize` compile with warnings as errors. The
fixture first executes actual non-null embedding/CreatePopup, then the exact
null-detach call used by the owner. It checks embedded/current context retirement,
closed window-callback admission, retained popup/platform release obligations,
actual ReleasePanel resource destruction, repeated detach/release, reattachment,
and a separately selected child context. Win32 windows, drawing contexts,
regions, timers and drag/drop registrations are represented by counted owned
resources. Before the fix, ASan/UBSan follows the actual setter through
CreatePopup(0) to the actual VPanel::IsPopup null read. This is provider-method
qualification on a native 64-bit host, not a full Windows provider TU/runtime or
real OS/UI acceptance claim. Source/generated-region/executable hashes are logged.
