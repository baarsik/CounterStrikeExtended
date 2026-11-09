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

The 12 CTest cases cover 11 bootstrap scenarios plus the existing three bounded
argument-entry checks. Bootstrap scenarios cover an unrelated working directory,
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
path storage. No process working-directory or real Steam settings are changed.
