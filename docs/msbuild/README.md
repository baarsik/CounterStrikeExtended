# Historical build provenance

This directory preserves upstream build evidence. Active runtime targets and
source inventories are maintained directly in CMake; see
[maintaining the build system](../build-system.md) and [building](../building.md).
The archived projects are not inputs to the active build.

## Archived originals

`upstream-manifests.zip` preserves 166 original non-external project, property,
item, solution, export, resource and filter manifests from upstream HEAD
`c10c1100792ce1c23f7941ff350f4c4746151ee1`. Entries retain their original bytes
and repository-relative paths, including references to removed games and SDL2.
They are historical comparison evidence, not current build instructions.

SHA-256: `fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2`

The 38 projects removed during runtime-scope pruning are listed in
[removed-projects.txt](removed-projects.txt). The current 19 production targets
are documented in [project scope](../project-scope.md#default-runtime).
Live Visual Studio projects and their translation/export machinery have been
removed; CMake definitions preserve the retained runtime settings.

Archive preservation does not establish Microsoft toolchain/user-property
evaluation. Extract into a separate copy of the original tree for comparison.
Removed source remains recoverable from the upstream Git commit. The inherited
Linux/macOS Makefiles have also been removed. Archived build prose describes
historical Carbon/i386 paths and provider requirements, not the supported build.

All inherited notices and rights apply unchanged to the archive contents. This
archive is not covered by the license for original build contributions.
