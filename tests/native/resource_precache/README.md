# Resource precache bounds

This component compiles the complete production `CL_PrecacheResources`,
`CL_PrecacheBSPModels`, optional index helper, and both HW/SW `Draw_DecalSetName`
bodies against actual SDK resource/client/cache declarations. Filesystem,
model/sound loading, allocation, diagnostics and UI services are controlled;
no game, renderer, Steam session or external server runs.

The cases cover zero and the last valid slot; negative, first-out-of-range,
12-bit wire-maximum and `INT_MAX` indices; missing sounds; lazy and inline
models; event filename/script loading; decal termination; unchanged client
state on invalid input; and continued traversal to valid resources after
malformed entries. Removing a production guard admits invalid input to a
loader/allocator, changes the captured client state, or triggers a sanitizer.

```sh
cmake -S tests/native/resource_precache -B build/resource-precache-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_RESOURCE_SANITIZERS=ON
cmake --build build/resource-precache-debug --parallel
ctest --test-dir build/resource-precache-debug --output-on-failure
```

Use a separate Release directory with `-DCMAKE_BUILD_TYPE=Release` and sanitizers
disabled. Windows uses the matching MSVC developer environment. Extraction
reruns when either production source changes; `CSE_RESOURCE_ENGINE` can select
a preserved engine source directory for negative controls. These tests exercise
the precache consumers, not complete resource-list decoding or game acceptance.
