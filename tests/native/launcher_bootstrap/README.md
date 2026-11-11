# macOS launcher bootstrap fixture

This executes all four launcher translation units selected by the retained
`launcher.vcxproj` evaluator. It loads an actual owned filesystem module and an
authored `IEngineAPI` engine from a temporary executable directory. It does not
start the game, Steam, a window, audio, or a microphone.

```sh
cmake -S tests/native/launcher_bootstrap -B build/launcher-bootstrap-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_BOOTSTRAP_FILESYSTEM=/absolute/path/to/filesystem_stdio.dylib
cmake --build build/launcher-bootstrap-debug --parallel 2
ctest --test-dir build/launcher-bootstrap-debug --output-on-failure -V
```

The bootstrap scenarios and bounded argument-entry checks cover an unrelated working directory,
Unicode and space-containing executable paths, a path over 260 bytes, relative
`argv[0]`, explicit game selection, executable alias, software-engine selection,
normal return, unload/restart, borrowed factory queries, retained peer ownership,
ABI mismatch before any filesystem factory call, and compatible/null-interface
cleanup. Some scenarios cover several of these properties together. The engine
checks the borrowed filesystem factory and reads a known ROOT-search-path file.

The Apple fixture uses `-fvisibility-inlines-hidden`, matching the approved
production target policy. Without it, the real Mach-O filesystem factory called
the executable's coalesced weak inline `InterfaceReg::Create` and returned null.
An actual LLDB trace and the crash are retained with the qualification evidence.
The Apple null-interface guard also prevents calling `Mount` on a null result.

Restart releases only the launcher's owned engine reference. A borrowed query of
the same executable-relative path must then report the image absent. A peer that
still owns the image makes the launcher clean up its filesystem and fail without
a second `Run`; the fixture verifies that the peer reference remains callable
and releases that reference itself. There is no force-close of borrowed handles.

`CSE_BOOTSTRAP_SANITIZERS=ON` instruments the actual launcher and probe. The real
filesystem and authored modules remain separately built, uninstrumented images;
this does not claim whole-module sanitizer coverage. Optional `--leaks` Python
runs cover normal return, restart and retained-peer cleanup. `leaks` reports its
own analysis exit status, so the direct CTest case separately verifies the
retained-owner launcher's failure status.

`windows_compile.py` consumes a frozen receipt produced by the owning manifest
evaluator. It initializes MSVC in RAM, compares real preprocessed Windows tokens
at the same physical source path, and compiles all four selected translation
units for x86, x64 and ARM64, Release and Debug, with `/W4 /WX`. It does not run a
Windows launcher. The Linux-selected source tokens are unchanged; Linux runtime
execution is outside this fixture.

The final evidence packet records exact source, selected inputs, binaries,
compiler flags, native CPU/minimum-OS identities, preserved failure receipts and
limits under `launcher-bootstrap-final` in the migration artifact directory.
Executable-relative module lookup is bounded by the existing 4096-byte launcher
path storage. The Apple launcher establishes that canonical runtime directory
after caller-relative response-file expansion and before loading modules or
reading `hl.conf`. Tests only change the child process cwd and owned temporary settings.

Set `CSE_BOOTSTRAP_RUNTIME=/absolute/path/to/complete/runtime` to add the actual
cohort/provider startup fixture. It generates exact hashes from the finished HW,
SW and VGUI images, stages the complete runtime, and compiles the actual retained
launcher/session. Only the call to the engine's `Run` method is replaced: real
filesystem, engine constructors, stock Steam export binding, root admission and
close remain active. The probe never calls `SteamAPI_Init` or starts game/UI/audio.
It checks no-argument startup from `/` and an unrelated directory, canonical
symlink aliases, spaces/Unicode, paths over 260 bytes, caller-relative response
expansion, and executable-owned renderer/window/size settings against conflicting
caller settings. Controlled OS errors cover executable-path and cwd failures
before settings mutation. Missing/incompatible providers and modified software
images fail closed; the latter is rejected before a missing provider could be
loaded by an engine constructor. This pre-Run fixture does not qualify a real
LaunchServices menu, live Steam authentication, normal game shutdown or per-user
writable-data separation; those remain controller acceptance work.
