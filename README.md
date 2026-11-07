# CounterStrikeExtended

A development fork of BlackShadow/GoldSrc-SDK retaining **Counter-Strike 1.6 and
Half-Life**, shared engine/UI/filesystem/demo runtime, and dedicated-server
support. The build targets Windows x86 with standalone MSVC Build Tools,
SDL3 and source-built libmpg123 music. [Current validation](docs/mpg123-migration-validation.md) records
earlier VM1 builds, music captures and gameplay checks, including coverage limits and an
unresolved startup failure. Native x64/ARM64 support remains future work.

The default Release build contains all 19 retained production targets, including
`hl.exe`, `hw.dll`, `hlds.exe`, `swds.dll`, both games' client/server DLLs,
filesystem, Tier0, VGUI/VGUI2, GameUI, particleman, Core and DemoPlayer.
Standalone HLTV broadcasting and A3D are included; reduced builds are explicit.
The approved 38 other-game, software-client, editor and SDK utility targets and unused game
implementation trees have been removed. Required compatibility headers live
with their retained consumers; inherited historical prose is kept under
`docs/upstream/`.

From an x86 MSVC command-line environment with the Windows SDK, CMake 3.25+
and Ninja, run (the Visual Studio IDE is not required):

```powershell
cmake --preset default
cmake --build --preset default --parallel
```

Use `runtime-x86-debug` for Debug or `hardware-x86-release` /
`hardware-x86-debug` for only the source-built hardware engine and VGUI1.
Release runtime files go directly into repository-root `output/`, with the actual
`cstrike/` and `valve/` game paths. Debug uses `output-debug/`; hardware-only builds
keep their output inside their build directory. Required intermediates remain under
`build/`. Release symbols/maps and dependency distribution materials default OFF.
Game assets and other external runtime providers are supplied separately; no live
game installation is modified.

- [Build commands, targets and provider requirements](docs/building.md)
- [Current project scope](docs/project-scope.md) and [source pruning record](docs/source-pruning.md)
- [MSBuild/CMake crosswalk](docs/msbuild-crosswalk.md) and [preserved original manifests](docs/msbuild/README.md)
- [C1–C4 source corrections and evidence limits](docs/source-corrections.md)
- [Future x64/ARM64 portability work](docs/architecture-portability.md)
- [Music and runtime validation](docs/mpg123-migration-validation.md)
- [Earlier SDL3 validation and benchmarks](docs/sdl3-migration-validation.md)

This repository is **not MIT as a whole**. MIT applies only to authorized original
contributions within the [root license scope](LICENSE). Inherited Valve/Id and
other components retain their existing notices, restrictions and unresolved
rights; see [the licensing audit](docs/licensing.md). No release or provider
redistribution is included in this work.
