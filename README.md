# CounterStrikeExtended

A development fork of BlackShadow/GoldSrc-SDK retaining **Counter-Strike 1.6 and
Half-Life**, shared engine/UI/filesystem/demo runtime, and dedicated-server
support. The initial build target is Windows x86 with MSVC v145. The CMake source
conversion is implemented; **Windows compilation and runtime qualification are
pending**. No x64/ARM64 support or playability is claimed.

The default complete-runtime build contains 16 production targets, including
`hl.exe`, `hw.dll`, `hlds.exe`, `swds.dll`, both games' client/server DLLs,
filesystem, Tier0, VGUI/VGUI2, GameUI, particleman, Core and DemoPlayer. Miles music
integration remains intact. Standalone HLTV broadcasting and A3D are preserved
as optional features, default OFF. The approved 38 other-game, software-client,
editor and SDK utility targets have been removed.

On a suitable Windows builder with Visual Studio 2026, v145, SDK 10.0.26100.0 and
CMake 4.2+, run:

```powershell
cmake --preset runtime-x86-release
cmake --build --preset runtime-x86-release --parallel
```

Use `runtime-x86-debug` for Debug or `hardware-x86-release` /
`hardware-x86-debug` for only the source-built hardware engine and VGUI1.
Outputs remain under the selected `build/` directory, separate from symbols,
import libraries and intermediates. There is no installation into a live game.

- [Build commands, targets and provider requirements](docs/building.md)
- [Approved project disposition](docs/project-scope.md) and [actual source pruning](docs/source-pruning.md)
- [MSBuild/CMake crosswalk](docs/msbuild-crosswalk.md) and [preserved original manifests](docs/msbuild/README.md)
- [C1–C4 source corrections and evidence limits](docs/source-corrections.md)
- [Future x64/ARM64 portability work](docs/architecture-portability.md)
- [Actual validation record](docs/validation.md)

This repository is **not MIT as a whole**. MIT applies only to authorized original
contributions within the [root license scope](LICENSE). Inherited Valve/Id and
other components retain their existing notices, restrictions and unresolved
rights; see [the licensing audit](docs/licensing.md). No release, provider
redistribution, game launch or runtime acceptance is included in this work.
