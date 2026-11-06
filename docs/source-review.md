# Independent review record

The report below reviews the pre-fix source at `44ef186`. Its two Important
findings and C-as-C++ translation issue were addressed in `8dcebd9`; see
[the post-fix validation record](validation.md). The report is retained as
original evidence, not relabeled as a second post-fix review or build result.
No tests/runtime execution was performed.

---

# Fresh whole-branch source review

Reviewed base `c10c1100792ce1c23f7941ff350f4c4746151ee1` through current HEAD `44ef186c33c5f31333b5df0515da0c01a76b8ef5` on 2026-11-06. The checkout was clean. Requirements were the external owner handoff, approved `docs/project-scope.md`, implementation plan, and recorded progress rulings. This report is the only review write. No subagents, configuration, compilation, tests/CTest, binaries, providers, workers, games, or network execution were used.

## Strengths

- The default/optional target graph retains both games, dedicated support, Miles, Core and DemoPlayer. Hardware-only uses the same production target and source-built VGUI1, including its compatibility import-library post-build command.
- Separate `cmake/targets/<target>` directories correctly bound source properties. Particleman's forced Blue Shift headers and include precedence do not leak into other targets compiling `public/interface.cpp`. Hardware's strict `common.cpp`, asynchronous `sys_dll2.cpp`, no-ICF policy, and Release assertion definitions are represented explicitly. CS assertion/iterator settings and the distinct static/DLL CRT families are preserved in the generated selections.
- C1/C4 affect the selected VGUI2 file and preserve valid mappings/reverse initialization. C2 explicitly pairs the two retained engine targets with WSOCK32; C3 narrowly changes the requested hardware terminal guards. Source changes match their stated verification limits.
- Pruning is bounded and recoverable. The 166 archived manifests match original Git bytes. The deletion inventory stays within the approved source roots apart from approved project/filter/solution removals. Retained selected compile/resource/library/project-reference paths and generated SDK path references exist. No remaining explicit relative include into a deleted file was found.
- Licensing language separates original contributions from inherited selections/source and leaves provider and reconstructed-source permissions unresolved. Documentation consistently distinguishes source work from unavailable Windows/runtime evidence.

## Issues

### Critical

None found.

### Important

1. **Shared output basenames collide in the common import-library and linker-PDB directories.**

   **Location:** `cmake/RuntimeTargets.cmake:14` and `:16`; callers `cmake/targets/cs_client/CMakeLists.txt:2`, `cmake/targets/hl_client/CMakeLists.txt:2`, and the launcher/Half-Life server targets.

   `OUTPUT_NAME` is the runtime basename, while every target uses the same archive and linker-symbol directories without an independent archive/PDB name. Both clients therefore produce `lib/<config>/client.lib` (and its export sidecar) and `symbols/<config>/client.pdb`. The launcher and Half-Life server similarly share `symbols/<config>/hl.pdb`. The separate runtime game directories and unique compile-PDB names do not separate these linker outputs. In the documented parallel complete-runtime build, the linkers can overwrite or contend for these files; even a serial build loses one module's symbols/import library. The original projects used separate `IntDir` directories and did not have this collision. Installed CMake documentation confirms that an unset `PDB_NAME` defaults to `OUTPUT_NAME`.

   **Remedy:** Give each target a unique archive and linker-PDB directory or set target-specific `ARCHIVE_OUTPUT_NAME` and `PDB_NAME`, while retaining the required runtime filenames/locations. Include executable exports in the import-library output audit. Check both complete-runtime configurations for distinct linker products.

2. **The optional A3D target omits five generated geometry translation units.**

   **Location:** `cmake/targets/a3dapi/CMakeLists.txt:9`; translation omission at `tools/translate_msbuild.py:129`–`:133`. Source of truth: `GoldSrc-SDK/projects/vs2026/a3dapi.vcxproj:193` (`PrepareGeometryCompilation`).

   The original project generates `geometry_objects.cpp`, `geometry_finalize.cpp`, `geometry_acoustics.cpp`, `geometry_submission.cpp`, and `geometry_state.cpp` before compilation, each defining a different `A3D_GEOMETRY_*` selector and including `A3dGeom.cpp`. The evaluator ignores `Target` elements and the CMake target only compiles that file with `A3D_GEOMETRY_CORE`. Consequently none of the other five implementation sections is compiled. For example, core `CA3dGeometryBuilder::AddVertex3f` calls `FinishPolygon` at `A3dGeom.cpp:1262`, but its implementation at `:2903` is under `A3D_GEOMETRY_FINALIZE`. Other missing sections supply object, submission, acoustic and state implementations. Enabling `CSE_BUILD_A3D` cannot produce the intended complete adapter and will encounter unresolved geometry symbols. This is a source-visible translation defect independent of missing Windows validation.

   **Remedy:** Model all five generated units as real CMake compile inputs with their section macros and individual optimization, instruction-set, FP, intrinsic and asynchronous-EH settings from the original target. Keep them target-local; compiling the same path repeatedly with shared source properties is insufficient. Extend the crosswalk to document these build-time generated selections, and make the translator reject unsupported source-generating project targets instead of silently omitting them. The evaluation-only exporter also cannot capture target-time additions, so those need an explicit crosswalk.

### Minor

1. **Translate `CompileAsCpp` into CMake's source language as well as compiler parsing.**

   **Location:** `tools/translate_msbuild.py:308`; generated `cmake/targets/hlds/CMakeLists.txt:106` and `:153`.

   The retained `engine/mem.c` overrides `CompileAs` to C++, but the translation only adds `/TP`. That tells MSVC to parse C++, while CMake still classifies the source as C by extension. In a single-config Ninja/MSVC build, CMake therefore does not apply `CXX_STANDARD 17` to this source. This is a language-policy mismatch, not an observed build failure: the file currently needs only simple C++ constructs, and the documented Visual Studio generator shares target-wide C/C++ flags. Set source `LANGUAGE CXX` in the hlds target directory so compiler selection, language standard and language-conditioned flags all agree with the original project.

## Inspection evidence and limits

- Read the generated target modules, common helpers/default hook, translator/exporter, presets, standalone entry point, modified runtime source, licensing/build/validation/portability documentation, approved scope and original relevant manifests.
- Independently checked all 166 archived manifest entries against base Git bytes: zero mismatches.
- Inspected all 1,404 deleted paths: the only deletion outside the 21 source roots after excluding project/filter manifests is the approved obsolete `GoldSrc-SDK/projects/vs2026/utils.sln`.
- Checked 2,592 active per-configuration compile/resource/library/project-reference selections: zero missing paths (this includes eight project references beyond the reported 2,584 compile/resource/library selections). All SDK paths extracted from generated CMake modules also exist.
- Checked retained source text for explicit relative includes into deleted files: none found. This is bounded lexical dependency evidence, not a Windows-preprocessed closure or build proof.
- Inspected compiler/CRT/assertion/FP/EH/library/export/resource metadata and the actual A3D source-generation target. No CMake configure or build was attempted to demonstrate the findings.

## Declined to judge

- Actual Windows generation, Debug/Release compilation, Microsoft toolchain defaults, emitted PE imports/exports/resources and ABI equivalence: no suitable builder was available, and review execution was expressly source-only; the handoff allows this evidence to remain pending.
- Runtime/playability, live multicast behavior, fatal-process observation, SDL key behavior, provider compatibility and worker shutdown: runtime/provider/network execution and tests were prohibited; source conformance does not prove these behaviors.
- Changing the inherited `SW_RENDERER`/missing-`SWDS_DLL` discrepancy: the progress ruling expressly preserves that upstream selection pending separate evaluation; it is not introduced by this branch.
- Redesigning inherited software-renderer selection UI/launcher paths: removal of `sw.dll` was approved and the current unavailable mode is explicitly documented; launcher redesign is outside C1–C4 and the approved implementation scope.
- Complete legal chain of title, binary redistribution clearance, or broad MIT eligibility: no such authority was supplied. Reviewed accuracy and boundedness of the repository's claims, not legal clearance or every inherited file's provenance.
- x64/ARM64 implementation and other operating systems: future inventory only; the current source entry points expressly reject unsupported builds.
- Reading every removed implementation body: the authorized review uses deletion boundaries, retained dependency inputs, preserved notices and archive provenance; source-pruning findings are limited accordingly.

## Assessment

**Spec verdict:** Substantially aligned, with two material CMake defects: complete-runtime linker products are not isolated, and optional A3D production inputs are incomplete.

**Quality verdict / ready to merge:** **With fixes.** Correct the two Important findings before accepting the source baseline. The lack of Windows validation is honestly reported and is an allowed handoff limitation, not itself a rejection reason; those pending checks still must precede any claim of a functioning Windows binary baseline.
