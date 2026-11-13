# CounterStrikeExtended

## C++ code style

- Apply these conventions to new and edited C++ code; keep formatting changes
  within the task's scope.
- Use four spaces for indentation, spaces after commas and around binary
  operators, and a 120-character line limit where practical. Put one statement
  per line and declare one variable per declaration.
- Use braces for control-flow bodies, including single-statement branches and
  loops. Put opening braces on the same line as the declaration or control
  statement, and expand nonempty bodies onto separate, indented lines. Separate
  functions and logical sections with blank lines.
- Use descriptive `constexpr` constants for domain values, scales, limits and
  sentinel values instead of unexplained numeric literals. Keep constants in
  the smallest appropriate scope, reuse existing constants, and derive bounds
  from containers where possible. Obvious literals such as zero initialization
  and unit increments do not need names. Preserve the exact value, type and
  precision required by the ABI and arithmetic; briefly explain a constant's
  meaning or evidence when it is not otherwise clear.

## Architecture

- The build targets Windows x86 (Win32), using C, C++ and Windows resources,
  MSVC Build Tools and the Windows SDK.
- Production source lives under `src/`. The runtime includes the engine,
  launchers, Counter-Strike and Half-Life client/server DLLs, and shared runtime
  libraries. Each game keeps its own runtime-relative client/server outputs.
- Root `CMakeLists.txt` composes the runtime. Target definitions under
  `cmake/targets/` use shared helpers in `cmake/RuntimeTargets.cmake`; per-file
  settings are scoped to each target directory. `hardware/CMakeLists.txt`
  provides the standalone hardware-engine entry point.

## Build workflow

- Run commands from the repository root in an x86 MSVC command-line
  environment with the Windows SDK, CMake 3.25 or later, and Ninja.
- Use the checked-in configure/build presets:

  ```powershell
  cmake --preset default
  cmake --build --preset default --parallel
  cmake --preset runtime-x86-debug
  cmake --build --preset runtime-x86-debug --parallel
  ```

- Use `hardware-x86-release` or `hardware-x86-debug` with the same commands for
  only the hardware engine and its source-built VGUI1 dependency.
- CMake is the build source of truth. Edit target settings in
  `cmake/targets/<target>/CMakeLists.txt` and source membership in the adjacent
  `Sources.cmake` directly. Keep shared policies in `cmake/RuntimeTargets.cmake`
  and per-file settings in the owning target directory. Regenerate derived
  production source files with `python3 tools/generate_runtime_sources.py`
  after changing their CMake-owned inputs. See [build-system.md](docs/build-system.md).
- Default Release builds all 19 retained production targets into repository-root
  `output/`, using the actual game-relative paths. Debug uses `output-debug/`.
  Hardware-only outputs remain inside their preset build directory.
- Keep required import/static libraries and intermediates in the selected build
  directory. Release diagnostics and distribution materials default OFF and must
  be explicitly enabled. Do not add non-runtime artifacts to the default output.
  See [building.md](docs/building.md) for target options and full commands.
