# Source pruning record

Paths in the removal tables are relative to `GoldSrc-SDK/`. The original 166
manifests remain byte-preserved in the [upstream archive](msbuild/README.md).
The [project scope](project-scope.md) describes the current runtime; the complete
38 removed project filenames are in [removed-projects.txt](msbuild/removed-projects.txt).
Exact source deletions remain in Git rather than a duplicate file inventory.

## Initial source stage — 2026-11-06

The approved targets were removed before pruning their exclusive source. The
retained/optional source selections, forced includes and conservative recursive
include closure had no consumers inside these 21 roots. Selected compile,
resource and library inputs were checked again after deletion; none was missing.

| Root | Files removed |
| --- | ---: |
| `gearbox/` | 299 |
| `ricochet/` | 186 |
| `czeror/` | 297 |
| `worldcraft/` | 438 |
| `utils/bspinfo/` | 3 |
| `utils/light/` | 9 |
| `utils/makefont/` | 3 |
| `utils/makels/` | 4 |
| `utils/mdldec/` | 6 |
| `utils/mdlviewer/` | 6 |
| `utils/mkmovie/` | 2 |
| `utils/procinfo/` | 4 |
| `utils/qbsp2/` | 15 |
| `utils/qcsg/` | 12 |
| `utils/qlumpy/` | 5 |
| `utils/qrad/` | 7 |
| `utils/serverctrl/` | 11 |
| `utils/smdlexp/` | 8 |
| `utils/studiomdl/` | 7 |
| `utils/visx2/` | 6 |
| `utils/xwad/` | 2 |

Total: **1,330 files removed**. Prose/notice candidates were initially retained
in place and later moved to the archive below. Shared engine/game code,
third-party dependencies, `utils/common`, `utils/sprgen` and software-renderer
source remain. Embedded notices and bundled dependency notices are unchanged.

## Residual game-source cleanup — 2026-11-07

The remaining `dod/`, `dmc/`, `tfc/` and `bshift/` trees were removed after
extracting the required headers. They contained 1,027 files; none was a retained
direct C/C++ compile input.

- Ten Blue Shift headers remain byte-for-byte under `particleman/compat/`.
  Particleman's forced includes and include precedence use that private
  directory, retaining its `BASEMONSTER_H` setting.
- The unchanged DoD `IClientVGUI` definition resides directly at
  `cstrike/public/game_controls/iclientvgui.h`, replacing the forwarding stub.
- Inactive other-game header/viewport branches were removed from shared source.
  No retained target defines the removed game selectors.
- Ten orphan `.def` files, two Deleted Scenes `.items` files and four legacy
  DMC/Ricochet makefiles were removed, along with their obsolete dispatcher
  targets/includes and unused property branches. Original MSBuild inputs remain
  in the unchanged manifest archive.
- Nine inherited prose files were moved unchanged to `docs/upstream/`, removing
  the remaining prose-only game/editor/tool directories from production source.

This stage removes **1,031 unused implementation/build files**, preserving eleven
required header bodies and nine prose files. Static comparison of all 38
retained project configurations confirms unchanged target properties/defaults,
compile/resource/library/project-reference selections and flags, except the
intentional particleman compatibility-path relocation. Particleman's lexical
closure remains 26 headers and 31 resolved include edges in each configuration.
Generated CMake and the crosswalk are synchronized, and all four presets parse.
Independent read-only review found no critical or important issues.

These are source checks. No tests or runtime code were executed for this cleanup;
Windows compilation and preprocessing have not been repeated. Earlier Windows
build/runtime results apply to the snapshot identified in the
[SDL3 validation report](sdl3-migration-validation.md).

## Inherited prose archive

These files preserve upstream text and provenance, including obsolete commands
and game/tool references. They are historical material, not current build
instructions, and do not acquire a new license through relocation.

| Former SDK path | Current repository path |
| --- | --- |
| `projects/vs2026/bshift_client.txt` | `docs/upstream/bshift-client-build.txt` |
| `dmc/cl_dll/readme.txt` | `docs/upstream/dmc-client-readme.txt` |
| `ricochet/cl_dll/readme.txt` | `docs/upstream/ricochet-client-readme.txt` |
| `worldcraft/editorkeys.txt` | `docs/upstream/worldcraft-editorkeys.txt` |
| `worldcraft/new16.txt` | `docs/upstream/worldcraft-new16.txt` |
| `worldcraft/whatsnew.txt` | `docs/upstream/worldcraft-whatsnew.txt` |
| `utils/qcsg/hulls.txt` | `docs/upstream/qcsg-hulls.txt` |
| `utils/qlumpy/qlumpy.doc` | `docs/upstream/qlumpy.doc` |
| `utils/qrad/qrad.txt` | `docs/upstream/qrad.txt` |
