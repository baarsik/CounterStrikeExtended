# Original MSBuild evidence

`upstream-manifests.zip` preserves 166 original non-external project, property, item, solution, export, resource and filter manifests from upstream HEAD `c10c1100792ce1c23f7941ff350f4c4746151ee1`. Entries retain their original bytes and repository-relative paths. No archive entry executes anything.

SHA-256: `fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2`

The 38 approved live project removals are listed in `removed-projects.txt`. Retained/optional projects and their imports remain live under `GoldSrc-SDK/`. Historical manifests remain in this archive throughout CMake translation; their preservation does not claim that Microsoft toolchain/user-property evaluation has run. Extract into a separate copy of the original tree for Windows MSBuild comparison, never over unrelated working changes. Removed source can be recovered from the upstream Git commit.

All inherited notices and rights apply unchanged to the archive contents. This archive is not covered by the license for original build contributions.
