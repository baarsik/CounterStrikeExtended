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
CMake validates the selected payload hashes.
`python3 tools/sdl3_dependency.py` additionally inspects PE/COFF x86 machines and
import-library DLL identity without loading or executing the binary.

Build integration belongs to the owning CMake target. Shared consumers call
`cse_link_sdl3(target, configuration)` from `cmake/SDL3Dependency.cmake`; this
links the pinned provider and shares an always-run staging target that restores
`SDL3.dll` in `CSE_OUTPUT_DIRECTORY`, including when game module outputs are
nested. Header-only consumers do not acquire import linkage or runtime staging.
Native runtime presets build the pinned SDL3 source instead of this x86 binary.

Run local payload and CMake build-contract checks with
`python3 -m unittest discover -s tools/tests -v`. These cover missing/mutated
payload rejection and deleted-DLL restoration. Actual platform build commands
and provider requirements are in the [build reference](../../../docs/building.md).
