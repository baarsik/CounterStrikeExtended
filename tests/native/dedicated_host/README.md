# Dedicated host boundaries

From the repository root on native macOS ARM64:

```sh
cmake -S tests/native/dedicated_host -B build/dedicated-host-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dedicated-host-debug
ctest --test-dir build/dedicated-host-debug --output-on-failure
```

Repeat in a fresh `dedicated-host-release` directory with `Release`.
These tests compile the real command parser, dedicated session, module owner,
common console editing core, POSIX console/signal owners and module loading code.
Controlled engine/filesystem modules record boundary calls. They do not simulate
successful gameplay, Steam authentication, GUI admission or full hlds acceptance.
The GUI boundary aborts if the console tests accidentally call it.

The suite checks exact 1023/1024 encoded command bounds, quoted UTF-8 and flag
presence; literal at signs in executable paths, passwords and quoted values through
the actual common command-line owner without response-file expansion; atomic
failure of literal initialization; final pipe line delivery and continued frames;
real PTY editing and restoration after SIGINT/SIGTERM; closed descriptors; partial signal installation;
fresh retry generations; shutdown/log/retirement/unmount/unload ordering; missing
retirement export; descriptor-role and pre-constructor SHA mismatch rejection.
The existing actual engine-owner extraction fixture additionally checks partial
filesystem initialization rollback, nested cleanup/retry, wrong-version/active
retirement, terminal retirement and exact engine command-copy bounds. A physical
negative control compiles a reordered real interface declaration and rejects its
compiler-reported slots.

`tools/dedicated_abi_evidence.py` is a separate, mandatory owning-build gate on
macOS `swds` and `hlds`. It compiles actual production translation units using
their compile database recipes and checks physical method slots, zero-offset
base views, eight-byte native interface objects and 32-bit admin words. Its
receipt binds actual sources, effective recipes, compiler identity and complete
preprocessed include contents. It runs on every owning build; no stale receipt
can skip validation. This does not promote the generic fixture-only bootstrap
prototype. The existing software/VGUI cohort supplies separate cold-image
checks; filesystem009 retains its separate typed contract. Runtime checks require
actual factory exports, provider-owned vtables and consumed slot addresses before
virtual dispatch; reverse host exports receive the same admission.

The production GUI selection deliberately fails before loading an unadmitted
standalone VGUI2 provider. `NativeGui` contains the retained panel/control owner
and lifecycle hooks for Tasks 2/3. Those tasks must supply its complete nine-view
provider admission and revocable admin binding; console is never a GUI fallback.
All original hlds responsibilities remain selected by the retained manifest.
Current full-graph blockers are AdminServer native files and configuration socket
headers. No GUI or full-hlds runtime acceptance is claimed.

Windows qualification uses real Windows APIs, not shims:

```powershell
cmake -S tests/native/dedicated_host/windows -B build/dedicated-conproc-x64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dedicated-conproc-x64-debug
ctest --test-dir build/dedicated-conproc-x64-debug --output-on-failure
```

Run in x86 and x64 MSVC environments, Debug and Release. The fixture repeats real
mapping/event/thread startup and joined shutdown, verifies borrowed inherited
handles remain valid, and checks full-width decimal handle parsing. The controller
owns these runs and actual Steam/game-server and physical GUI qualification.
