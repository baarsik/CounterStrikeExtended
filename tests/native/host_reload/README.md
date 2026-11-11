# Failed-load reload lifecycle

This CPU fixture extracts complete production load/reload, command enqueue/drain,
disconnect/clear-memory and ED_Alloc owners. Unique anchors and full source hashes
are recorded in the build's `owners.json`. The archive reader is included directly;
filesystem leaves and independent archive bytes come from the save_archive fixture.

SV_SpawnServer is a controlled allocation boundary: it creates inactive server
edicts, or fails without them. The controlled world callback drains game-rule
commands before calling real ED_Alloc, as Half-Life world precache does. The hunk
leaf retires actual fixture memory. Envelope parsing, successful Host_Map, active
server shutdown, audio, renderer, networking and other unrelated leaves are
substituted. These tests establish CPU recovery/lifetime behavior, not actual game
or campaign restore compatibility.

The regression queues `reload` followed by an unrelated command. Header, strict
archive validation and extraction failures must restart the map without scheduling
a disconnect that destroys the new edicts during nested command execution. Explicit
Host_Load failures retain their existing UI/disconnect behavior. Missing/no recent
saves, successful canonical archive loading, repeated cycles and failed fallback
spawn are covered. Archive EOF policy is unchanged.

Run serially from the repository root; use separate Debug, Release and sanitizer
build directories. On macOS select the lexical compiler path `/usr/bin/clang++`.

```sh
cmake -S tests/native/host_reload -B build/host-reload-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build build/host-reload-debug --parallel 1
ctest --test-dir build/host-reload-debug --parallel 1 --output-on-failure
```

Set `CSE_RELOAD_SANITIZE=ON` for scoped ASan/UBSan. Reverting only the production
fix reproduces `ED_Alloc: No edicts yet` in the failing reload cases and shows game
callbacks entered after failed spawn. No original save, device or live app is used.
