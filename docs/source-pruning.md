# Approved staged source pruning

Owner approved the 38 target removals and 21 candidate roots on 2026-11-06.
Target removal was committed before source pruning. The original 166 manifests remain byte-preserved in `msbuild/upstream-manifests.zip`.

Before deletion, the retained/optional source selections, forced includes and conservative recursive include closure had no consumers inside these 21 roots. All selected compile/resource/library inputs were checked again after deletion; none is missing. This is source-dependency evidence, not Microsoft build/runtime acceptance.

Only files under these approved roots were pruned. Prose/notice/provenance candidates were retained in place. No blanket third-party, shared game, Blue Shift, DoD, DMC, TFC, utils/common, utils/sprgen or software-renderer-source pruning occurred.

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
