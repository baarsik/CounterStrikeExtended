# Approved staged source pruning

Owner approved the 38 target removals and 21 candidate roots on 2026-11-06.
Target removal was committed before source pruning. The original 166 manifests remain byte-preserved in `msbuild/upstream-manifests.zip`.

Before deletion, the retained/optional source selections, forced includes and conservative recursive include closure had no consumers inside these 21 roots. All selected compile/resource/library inputs were checked again after deletion; none is missing. This is source-dependency evidence, not Microsoft build/runtime acceptance.

The first stage pruned only those approved roots and retained prose/notice candidates in place. The follow-up below records the owner-authorized removal of residual other-game trees and relocation of their required dependencies. Shared engine/game code, third-party dependencies, utils/common, utils/sprgen and software-renderer source remain.

| Approved root | Files removed in source stage | Preserved prose/notices |
| --- | ---: | --- |
| `gearbox/` | 299 | None found by filename/prose-extension inspection |
| `ricochet/` | 186 | `ricochet/cl_dll/readme.txt` |
| `czeror/` | 297 | None found by filename/prose-extension inspection |
| `worldcraft/` | 438 | `worldcraft/editorkeys.txt`, `worldcraft/new16.txt`, `worldcraft/whatsnew.txt` |
| `utils/bspinfo/` | 3 | None found by filename/prose-extension inspection |
| `utils/light/` | 9 | None found by filename/prose-extension inspection |
| `utils/makefont/` | 3 | None found by filename/prose-extension inspection |
| `utils/makels/` | 4 | None found by filename/prose-extension inspection |
| `utils/mdldec/` | 6 | None found by filename/prose-extension inspection |
| `utils/mdlviewer/` | 6 | None found by filename/prose-extension inspection |
| `utils/mkmovie/` | 2 | None found by filename/prose-extension inspection |
| `utils/procinfo/` | 4 | None found by filename/prose-extension inspection |
| `utils/qbsp2/` | 15 | None found by filename/prose-extension inspection |
| `utils/qcsg/` | 12 | `utils/qcsg/hulls.txt` |
| `utils/qlumpy/` | 5 | `utils/qlumpy/qlumpy.doc` |
| `utils/qrad/` | 7 | `utils/qrad/qrad.txt` |
| `utils/serverctrl/` | 11 | None found by filename/prose-extension inspection |
| `utils/smdlexp/` | 8 | None found by filename/prose-extension inspection |
| `utils/studiomdl/` | 7 | None found by filename/prose-extension inspection |
| `utils/visx2/` | 6 | None found by filename/prose-extension inspection |
| `utils/xwad/` | 2 | None found by filename/prose-extension inspection |

Total source-stage files removed: **1330**. The exact deletion list is recorded by the source-pruning Git commit, avoiding a second file inventory that can drift. Embedded rights notices in retained files and bundled dependency notices are unchanged. Removed implementation remains recoverable at the original upstream Git commit.

The 38 historical project filenames and their outputs/dispositions remain documented in `project-scope.md` and `msbuild/removed-projects.txt`. Their deletion does not remove any retained compile input.

## Residual game-source cleanup — 2026-11-07

The owner authorized removing unused residual game files to reduce upgrade noise.
The remaining `dod/`, `dmc/`, `tfc/` and `bshift/` trees have been removed. They
contained 1,027 files; none was a retained direct C/C++ compile input.

- Ten Blue Shift headers are retained byte-for-byte under `particleman/compat/`.
  Particleman's forced includes and include precedence now use that private
  directory. Its lexical closure remains 26 headers and 31 resolved include
  edges in each configuration.
- The unchanged DoD `IClientVGUI` header now resides directly at
  `cstrike/public/game_controls/iclientvgui.h`, replacing its forwarding stub.
- Inactive other-game header/viewport branches were removed from the affected
  shared source. No supported target defines the removed game selectors.
- Ten orphan `.def` files, two Deleted Scenes `.items` files and four legacy
  DMC/Ricochet makefiles were removed; obsolete game targets/includes in the
  legacy make dispatcher and unused removed-project property branches were also
  removed. Original MSBuild inputs remain in the unchanged manifest archive.
- Nine inherited prose files, including the DMC/Ricochet client readmes, the
  obsolete Blue Shift build note and legacy editor/tool documentation, are
  preserved unchanged under repository-root
  `docs/upstream/`. The `ricochet/` and `worldcraft/` production-tree remnants and
  prose-only removed-tool directories are therefore gone as well. These archived
  files are historical references, not current build instructions.

This removes 1,031 unused implementation/build files while preserving the eleven
required header bodies and nine prose files. Source inspection of all 38
retained project configurations confirms unchanged target properties/defaults,
compile/resource/library/project-reference selections and flags, except the
intentional particleman compatibility-path relocation. Generated CMake and the
crosswalk have been regenerated. Tests/runtime execution remain disabled;
Windows compilation and preprocessing are unverified for this cleanup.

An independent read-only review confirmed the retained selections and header
closure, with no critical or important findings. Its two stale-documentation
findings were corrected. No tests or runtime code were executed for the review.
