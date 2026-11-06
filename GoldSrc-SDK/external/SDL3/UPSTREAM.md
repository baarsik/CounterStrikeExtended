# SDL3 3.4.20, official Visual C++ x86 package

- Upstream: https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20
- Archive: https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-devel-3.4.20-VC.zip
- Archive SHA-256: `1ec3009655ee37b1f73e52b3b120acc5afd8abaf9532f3303b1bf1dfe91b5969`.
- License: unmodified upstream `LICENSE.txt` (zlib).

Selected files are the complete `include/SDL3/` tree; `lib/x86/SDL3.lib` and
`SDL3.dll`; `cmake/SDL3Config.cmake`, `SDL3ConfigVersion.cmake`, and `sdlcpu.cmake`;
and upstream `INSTALL.md`, `LICENSE.txt`, and `README.md`. All selected bytes are
unmodified. `SHA256SUMS` lists every selected file as `sha256|relative/path`.
`UPSTREAM.md` and `SHA256SUMS` are repository-owned provenance and validation data.
The archive stays outside production source. PDB, SDL3_test, x64 and ARM64 payloads
are intentionally omitted. There is no SDLmain library or SDL2 compatibility layer.

Debug and Release consumers use the same upstream x86 shared DLL/import library;
this does not claim that SDL was rebuilt using this project's Debug flags.
CMake, standalone MSBuild, and translation validate the selected payload hashes.
`python tools/sdl3_dependency.py` additionally inspects PE/COFF x86 machines and
import-library DLL identity without loading or executing the binary.

Build integration is opt-in per consumer: set `CseSdl3Shared=true` before importing
`projects/vs2026/SDL3.props`, retain inherited include/link metadata, then import
`SDL3.targets` after `Microsoft.Cpp.targets`. With `CseSdl3Shared=false` (the
default), the contract supplies headers and validation without import linkage or
runtime staging. Shared consumers stage to `bin/Win32/$(Configuration)/`, including
clients whose own outputs are nested. `CseValidateSdl3` can also run explicitly.
The translator checks the exact shared files before recognizing their actions;
intentional contract edits must update `SDL3_BUILD_HASHES` and their tests.
CMake translation emits `cse_link_sdl3(target, configuration)` and shares one
always-run staging target for `build/<preset>/runtime/<Configuration>/SDL3.dll`.

Run local source/build-contract checks with
`python -m unittest discover -s tools/tests -v`. On the Windows x86 builder, run
`tools/tests/test_sdl3_dependency.ps1 -EvidenceDirectory <fresh-local-directory>`
for isolated missing/mutated-provider checks, native x86 Debug/Release link probes,
and CMake/MSBuild deleted-DLL restoration. The fixture does not execute SDL or a game.
