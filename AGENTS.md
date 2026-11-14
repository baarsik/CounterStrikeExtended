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

- Runtime builds support Windows x86/x64 and macOS ARM64. Standalone AMXX modules
  target Windows x86 and Linux x86; component presets do not imply full runtime support.
- Production source lives under `src/`. Shared code can affect Counter-Strike,
  Half-Life, dedicated servers and HLTV; check the consumers affected by a change.

## Build maintenance

- CMake is the build source of truth. Root `CMakeLists.txt` composes the runtime;
  edit target settings in `cmake/targets/<target>/CMakeLists.txt` and source membership
  in the adjacent `Sources.cmake`. Keep shared policies in `cmake/RuntimeTargets.cmake`
  and per-file settings in the owning target directory.
- Run build commands from the repository root using the preset and toolchain matching
  the target platform, architecture and configuration. See [building.md](docs/building.md)
  for requirements, commands, dependencies and output paths.
- When changing inputs to generated ABI layouts, saved callbacks or exports, follow
  the [generation workflow](docs/build-system.md#generated-production-inputs). Edit the
  inputs, regenerate affected files and review both together instead of editing generated output.
- Keep import/static libraries and intermediates in the selected build directory;
  preserve game-relative runtime paths and keep non-runtime artifacts out of runtime output.

## Compatibility and scope

- Preserve exported interfaces, calling conventions, target-specific structure layouts
  and network/save/demo formats. Make intentional compatibility changes explicit and verify them.
- When moving sources or deployment resources, update CMake membership, packaging,
  tooling and documentation references as applicable.
- Preserve dependency pins, upstream notices and provenance. Keep dependency changes
  explicit and relevant to the task.
