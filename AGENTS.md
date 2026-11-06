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

- The current build targets Windows x86 (Win32), using C, C++ and Windows
  resources. It requires MSVC v145 and Windows SDK 10.0.26100.0.
- Production source lives under `GoldSrc-SDK/`. The runtime includes the engine,
  launchers, Counter-Strike and Half-Life client/server DLLs, and shared runtime
  libraries. Each game keeps its own runtime-relative client/server outputs.
- Root `CMakeLists.txt` composes the runtime. Target definitions under
  `cmake/targets/` use shared helpers in `cmake/RuntimeTargets.cmake`; per-file
  settings are scoped to each target directory. `hardware/CMakeLists.txt`
  provides the standalone hardware-engine entry point.

## Build workflow

- Run commands from the repository root on a Windows builder with Visual Studio
  2026, MSVC v145, the required SDK, and CMake 4.2 or later.
- Use the checked-in configure/build presets:

  ```powershell
  cmake --preset runtime-x86-release
  cmake --build --preset runtime-x86-release --parallel
  cmake --preset runtime-x86-debug
  cmake --build --preset runtime-x86-debug --parallel
  ```

- Use `hardware-x86-release` or `hardware-x86-debug` with the same commands for
  only the hardware engine and its source-built VGUI1 dependency.
- Generated target definitions and the MSBuild crosswalk come from
  `tools/translate_msbuild.py`. Update the retained manifests or translator as
  appropriate, then run `python tools/translate_msbuild.py`; keep generated
  outputs synchronized rather than editing them independently.
- Keep runtime binaries under `build/<preset>/runtime/<Configuration>/`, import
  libraries under `lib/<Configuration>/<target>/`, and PDBs under
  `symbols/<Configuration>/<target>/` within the same preset build directory.
  See [building.md](docs/building.md) for target options and full commands.
