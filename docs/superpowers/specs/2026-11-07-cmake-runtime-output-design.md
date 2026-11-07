# CMake runtime build design

Approved in chat on 2026-11-07.

Use CMake with Ninja, standalone MSVC Build Tools and the Windows SDK. Remove the Visual Studio IDE requirement, VS2026 generator selection, v145 check and exact SDK pin. Keep Windows x86 and the existing MSVC-compatible dependency ABI.

A default build is Release and compiles all 19 retained production targets, including dedicated servers, standalone broadcasting and A3D. Release runtime files go directly under repository-root `output/`, preserving `cstrike/` and `valve/` paths. Explicit Debug presets use a separate output directory. Existing game assets and external runtime providers are supplied separately.

Release does not generate debug symbols, maps or incremental-link files. Necessary compiler and linker intermediates remain under `build/`. Tests, diagnostics and distribution materials require explicit opt-in. Default output contains runtime files only; distribution remains a separate operation requiring the existing license/source materials.

CMake/Ninja is preferred over NMake for parallel builds. Continuing with a Visual Studio generator preserves IDE/version coupling; replacing MSVC would require a broader source/dependency port. Neither is part of this change.
