# HLTV polymorphic object lifetime checks

This packet checks the actual `NetAddress`, `BitBuffer`, `World::deltaCache_t` and `World::frameCache_t`
declarations and implementations. No World/Network class is replaced. It never runs Network::Init, DNS,
sockets, a provider, HLTV, or a game. No GUI or user terminal is used.

`extract_regions.py` copies the exact affected production reset/allocation/release regions into a controlled
function scope using the actual SDK types. Only allocator/error bindings and surrounding function parameters
are controlled. `generated_regions.json` pins every copied region and its source SHA. The actual BitBuffer,
NetAddress and byteorder CPPs are compiled normally; allocation hooks observe owned frees and reject frees
of external storage. `protected` access is made public only in the observation TU to inspect the actual
BitBuffer fields; no field/method/type/layout is substituted. Destructor calls live in a separate TU without
LTO; ARM64/x86_64 disassembly receipts confirm actual indirect dispatch.

Seven process-isolated cases cover constructor payload/dispatch, Network address reset and its six-byte wire
golden, normal cache allocation, each allocation failure, both failures, and 64 reset cycles mixing owned and
external BitBuffer storage. They check all scalar/ownership/endianness state, exact POD frame bytes, unchanged
hit/fault resets, idempotent release and destruction of live buffers at release. They deliberately do not
qualify full World initialization/shutdown, cache lookup algorithms or runtime networking.

Example Mac commands, from repository root:

```sh
ART=/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/hltv-object-reset-v1
python3 tests/native/hltv_object_reset/run_mac.py --recipes "$ART/actual-tu-red.json" --output "$ART/recheck"
python3 tests/native/hltv_object_reset/verify_source.py --before "$ART/before" --output "$ART/source-recheck.json"
python3 tests/native/hltv_object_reset/compile_tus.py \
  --release-database /Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/macos-runtime-policy-release/compile_commands.json \
  --debug-database /Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/macos-runtime-policy-debug/compile_commands.json \
  --output "$ART/tus-recheck"
```

`run_mac.py --before --source-root "$ART/before"` executes the frozen negative control. Its runtime build
omits global `-Werror` so the original invalid memset can execute, retaining visible warnings; strict original
whole-TU REDs are separately pinned. Final production and positive fixtures keep the owning warning policy.
Component profiles are C++14 ARM64 Release/Debug/ASan+UBSan and x86_64 Release/Debug; actual complete TUs
retain generated C++17 profiles. Source checks prove headers, layouts, BitBuffer and other algorithms unchanged.

`stage_windows.py` freezes the literal SDK include closure. `run_windows.py` uses it in an existing MSVC
x86/x64/ARM64 environment, running before/after C++14 Release/Debug fixtures and after-only complete C++17
production TUs. Receipts pin compiler path/SHA, exact commands, source SHA, expected outcomes and compiled
artifact hashes. These standalone owning profiles are distinct from root's full Ninja module rebuild.

The required production membership is existing Network.cpp, World.cpp and common/NetAddress.cpp only.
Network/World belong to hltv_core; NetAddress also belongs to hltv_proxy. No headers/manifests/build policies
change. Cache-size overflow validation, pre-Init World state and full module/runtime acceptance remain outside
this repair. These are intentional shared corrections; Windows selected-token invariance is not claimed.
