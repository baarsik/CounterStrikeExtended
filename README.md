# CounterStrikeExtended

A development fork of BlackShadow/GoldSrc-SDK for **Counter-Strike 1.6 and
Half-Life**, including their engine, shared runtime and dedicated servers.
The build targets Windows x86 with SDL3 and source-built libmpg123 music.
Native x64/ARM64 support remains future work.

From the repository root, use an x86 MSVC command-line environment with the
Windows SDK, CMake 3.25+ and Ninja:

```powershell
cmake --preset default
cmake --build --preset default --parallel
```

The default Release build compiles all 19 production targets into `output/`,
preserving the game's `cstrike/` and `valve/` paths. Game assets and external
runtime providers are supplied separately. Build intermediates stay in `build/`;
Release symbols, maps and distribution materials are opt-in.

Use `runtime-x86-debug` for Debug output in `output-debug/`, or
`hardware-x86-release` / `hardware-x86-debug` for the hardware engine and its
source-built dependencies. Tests and runtime qualification run separately.

- [Build commands and options](docs/building.md)
- [Runtime targets and project scope](docs/project-scope.md)
- [Build manifest reference](docs/msbuild/README.md) and [generated crosswalk](docs/msbuild-crosswalk.md)
- [Music and runtime validation](docs/mpg123-migration-validation.md)
- [Source pruning](docs/source-pruning.md), [source corrections](docs/source-corrections.md) and [portability work](docs/architecture-portability.md)

The runtime validation records earlier VM1 builds, gameplay and music checks,
including an unresolved startup failure. Full Windows Ninja compilation and
EXE/DLL inspection for the current build configuration remain pending.

This repository is **not MIT as a whole**. MIT covers authorized original
contributions within the [root license scope](LICENSE). Inherited Valve/Id and
other components retain their terms; see [licensing](docs/licensing.md).
