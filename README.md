# CounterStrikeExtended

A development fork of BlackShadow/GoldSrc-SDK for **Counter-Strike 1.6 and
Half-Life**, including the engine, shared runtime, dedicated servers and HLTV.
It uses SDL3 and source-built libmpg123 music.

**Win32 remains the default and regression reference.** Windows x64 and macOS
Apple Silicon migration is in progress. Windows x64 builds all 19 targets and
has selected gameplay/server checks; complete native acceptance is unfinished.

From the repository root, use an x86 MSVC command-line environment with the
Windows SDK, CMake 3.25+, Ninja and Python:

```powershell
cmake --preset default
cmake --build --preset default --parallel
```

Release runtime files go to `output/`, preserving `cstrike/` and `valve/` paths.
Use `runtime-x86-debug` for `output-debug/`. Game assets and external providers
are supplied separately; libraries, symbols and intermediates stay in `build/`.

- [Build commands and options](docs/building.md)
- [Native migration, requirements and TODO](docs/native-migration.md)
- [Validation status](docs/validation.md) and [component fixtures](tests/native/README.md)
- [Runtime targets and scope](docs/project-scope.md)
- [Build manifests](docs/msbuild/README.md) and [generated crosswalk](docs/msbuild-crosswalk.md)

This repository is **not MIT as a whole**. MIT covers authorized original
contributions within the [root license scope](LICENSE). Inherited Valve/Id and
other components retain their terms; see [licensing](docs/licensing.md).
