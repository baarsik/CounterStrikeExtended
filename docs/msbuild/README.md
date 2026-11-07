# Original MSBuild evidence

`upstream-manifests.zip` preserves 166 original non-external project, property,
item, solution, export, resource and filter manifests from upstream HEAD
`c10c1100792ce1c23f7941ff350f4c4746151ee1`. Entries retain their original bytes
and repository-relative paths, including references to removed games and SDL2.
They are historical comparison evidence, not current build instructions.

SHA-256: `fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2`

The 38 removed projects are listed in [removed-projects.txt](removed-projects.txt).
The 19 retained/optional projects and their imports remain live under
`GoldSrc-SDK/`; their current selections are in the
[generated crosswalk](../msbuild-crosswalk.md). Archive preservation does not
establish Microsoft toolchain/user-property evaluation. Extract into a separate
copy of the original tree for comparison. Removed source remains recoverable
from the upstream Git commit.

All inherited notices and rights apply unchanged to the archive contents. This archive is not covered by the license for original build contributions.
