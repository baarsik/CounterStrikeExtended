# HLTV native console component checks

These fixtures exercise the retained `CTextConsole`, `ObjectList` and `TokenLine` implementations, the new
macOS console/backend helpers, and complete `System.cpp` / `TextConsole.cpp` translation units. They do not
launch HLTV, a provider, a game, a network socket or a user's terminal.

The backend driver supplies a controlled `IBaseSystem`, `System_Printf`, and the exact retained
`Mem_ZeroMalloc(unsigned int)` allocation binding. Error injection wraps synchronous descriptor calls. PTYs
and pipes are created and owned by the Python runner. Input has one consumer; duplicated stdin/stdout may
share an open file description. The backend never sets file status flags. Raw descriptor flags and every
termios field are retained in protocol logs. Only independently measured Darwin kernel additions
`FWASWRITTEN` and `PENDIN` are accepted; input is never flushed to satisfy a test.

`history_tests.cpp` links the actual base implementation and ObjectList without any POSIX backend/helper.
It covers 29/30/31/59/60 entries, ring traversal, saved drafts, the existing end-caret restoration policy,
Enter/empty input, and editing after restoration. The inherited full-ring browse depth remains 29 entries.
The two history bound repairs intentionally also apply on Windows; the public class header is unchanged.

`helper_tests.cpp` uses real TokenLine storage and owned ordinary paths/symlinks. The module-loader callback
only checks placeholder files and returns an opaque controlled handle. Clock/sleep failures use typed
controlled functions; signal-lifetime checks also install/restore real handlers inside fixture processes.

Run from the repository root with the pinned command inventory from the evidence packet:

```sh
ART=/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/hltv-console-v1
python3 tests/native/hltv_console/run_backend.py --commands "$ART/actual-tu-red.json" --output "$ART/backend-recheck"
python3 tests/native/hltv_console/run_helpers.py --commands "$ART/actual-tu-red.json" --output "$ART/helper-recheck"
python3 tests/native/hltv_console/compile_tus.py --pinned-recipes "$ART/actual-tu-red.json" --output "$ART/tus-recheck"
python3 tests/native/hltv_console/verify_source.py --before "$ART/before" --output "$ART/boundary-recheck.json"
```

The component runners explicitly use C++14 with ARM64 Release/Debug/ASan+UBSan and x86_64 Release/Debug.
The TU runner preserves the pinned actual C++17 target recipe for existing sources, and clearly labels
new-source recipes derived from that target profile until root metadata integrates their selection.
The Windows runner accepts frozen before/after SDK include closures under `--source-stage`; it uses the
active MSVC environment, C++14, owning warning/exception/FP flags and Release/Debug CRT profiles. Each
architecture's receipt pins the compiler executable path/SHA and all compile/run commands.

Integration still needs platform source selection: retain `TextConsoleWin32.cpp` only on Windows and select
`TextConsolePosix.cpp` plus `PosixSystem.cpp` for macOS. No repository build policy is changed by this packet.
The full executable link, real providers/VGUI, server/broadcast operation, and shared +command quoting/ASCII
filtering are outside this qualification. Native `System::GetPanel` stays a concrete method, matching the
existing native IBaseSystem contract; no native base-interface slot is added.
