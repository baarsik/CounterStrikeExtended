# Retained dedicated administration source tests

These tests compile the retained production implementations with controlled engine services. They do not initialize Steam, open network sockets, start audio, or operate the shared GUI.

`extract_admin.py` copies the complete `CRemoteServer` class and its method bodies verbatim from `AdminServer.h/.cpp` to isolate packet dispatch from unrelated GUI widgets. `provider_tests.cpp` compiles the complete actual `sv_remoteaccess.cpp` and substitutes only its engine-service dependencies. `roundtrip_tests` connects those two actual owners. `extract_provider_state.py` copies the actual snapshot functions and supplies controlled owner/provider values: it proves no provider access before initialization/after retirement and no checkbox-derived secure result, **not authentication acceptance**. File operations use real temporary directories and files, including UTF-8 names, symlinks, filters, empty directories, roots, and permission errors. Permission denial is asserted when the runner is not root.

Run serially under the shared compiler allocation:

```sh
cmake -S tests/native/dedicated_admin -B build/dedicated-admin-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dedicated-admin-debug --parallel 1
ctest --test-dir build/dedicated-admin-debug --output-on-failure
cmake -S tests/native/dedicated_admin -B build/dedicated-admin-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/dedicated-admin-release --parallel 1
ctest --test-dir build/dedicated-admin-release --output-on-failure
```

For sanitizer lifetime checks, use a separate Debug directory with `-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`.

Native production settings use `SDL_GetPrefPath("CounterStrikeExtended", "hlds")`. Controlled GUI fixtures must explicitly set `CSE_DEDICATED_CONFIG_DIR` to an isolated absolute directory. Immutable platform resources remain in the runtime VFS; the override does not authorize writes into original game assets.

Full owning hlds builds, Windows owner builds, actual CS/HL GUI interactions, real provider authentication, restart and physical input remain separate controller qualification gates. No component fixture replaces them.

The actual-frame fixture compiles `AdminServer.cpp` and `AdminFiles.cpp` from the current checkout while reusing the owning control objects. Its ban-import checks select UTF-8 paths, `/`, empty directories, and missing directories through the real path combo box. They verify refreshed rows, the active file filter, stale filename clearing, and Open-button state. The filesystem fixture also covers dangling symlinks so one missing target cannot prevent an otherwise readable directory from being listed.

An optional actual-frame fixture links the completed owning `hlds` admin/control objects and opens only dummy-SDL windows. Copy retained core platform resources into a new fixture directory and overlay this repository's `src/deploy/platform/Admin` directory. Configure with `-DCSE_ADMIN_OWNER_BUILD=/absolute/owning/build -DCSE_ADMIN_RESOURCE_ROOT=/absolute/copied/platform`. `actual_admin_frame_close_reopen` checks three complete admin frame generations, rejects a high invalid public handle, retains real provider request/response flow and log insertion, destroys popup children, and writes `DialogConfig.vdf` only below that test build's `isolated-config` directory. It does not drive the physical GUI, authenticate a provider, or claim visual layout qualification.

`retired_gui_preserves_new_session` compiles the production `NativeGui::Stop` body with controlled inactive GUI dependencies and the real remote owner. It verifies that an already retired GUI cannot revoke the next session, while an active owner's repeated stop is idempotent.

Request IDs remain signed 32-bit and are never reused during the remote owner's lifetime; exhaustion rejects new requests. Rebinding drains the prior provider queue before admitting new listeners because update packets have no wire generation field. Oversized requests are rejected before execution, and malformed responses cannot consume a pending callback. Provider snapshots and callback-error tests use controlled values and make no authentication claim.

Required native runtime resources are selected by `cse_runtime_assets` in `cmake/targets/hlds/CMakeLists.txt`. The native owning `hlds` build stages these 16 files independently of optional distribution materials and restores deleted runtime copies on incremental builds. `resource_tests.py /absolute/runtime/root` queries the same CMake selection and compares every staged file with its selected source hash.

The optional `actual_setup_selection`, `actual_setup_text`, `actual_setup_config`, and `actual_setup_bounds` cases compile the complete production setup page, NativeGui, and DedicatedSession against actual controls and the owning VFS/standalone images, with a controlled module/engine boundary. They capture the exact Init argument, canonical restart arguments, secure retry, final 1023/1024-byte and 49/50-argument limits, full text-control input including UTF-8 byte boundaries, unchanged settings after rejected input, and writable/unwritable CONFIG admission. They never load the real engine or Steam SDK. Fixtures create their own game metadata/maps and CONFIG directories and use copied read-only platform resources. No clipboard actions or shared preference setters are exercised. The CONFIG override isolates VFS only.

The mapcycle roundtrip covers missing, empty, and nonempty-to-empty files with actual producer and consumer bodies. Test-only access exposes the existing pending-registration container to assert that completed empty replies do not accumulate requests; no production accessors or ABI changes are added. Controlled file allocations are counted and actually freed.
