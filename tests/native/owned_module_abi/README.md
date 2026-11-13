# Owned module ABI inventory spike

This directory is a read-only design spike. It publishes no production contract
and does not link, load, or execute a module. The scripts use the actual retained
19-target CMake inventory and current macOS compiler recipes. The CMake metadata
query does not cross-compile or qualify Windows binaries or external providers. Substituting a probe
source into an owning recipe is explicitly a **header probe**, not a full
production translation-unit qualification.

```sh
python3 tests/native/owned_module_abi/inventory.py \
  --mac-database "$ART/macos-runtime-policy-release/compile_commands.json" \
  --output "$ART/owned-module-abi-v1"
python3 tests/native/owned_module_abi/compile_probes.py \
  --database "$ART/macos-runtime-policy-release/compile_commands.json" \
  --output "$ART/owned-module-abi-v1/probes-final"
python3 tests/native/owned_module_abi/interface_probes.py \
  --database "$ART/macos-runtime-policy-release/compile_commands.json" \
  --output "$ART/owned-module-abi-v1/interface-probes-v1"
```

`ART` is the already authorized artifact directory, not a production path.
The receipt files retain the complete commands, compiler/database identity,
exit status, generated source, LLVM output, AST output and compiler layout dumps.
Earlier probe directories preserve an initial probe-harness include error.

- `capacity_probe.cpp` checks the real runtime9, 288-byte descriptor's 16-entry
  bound, compatible family additions, exact-check behavior, and rejection of
  missing/duplicate/oversized catalogs. Test-only family IDs are not ABI IDs.
- `signature_probe.cpp` uses the actual 14 lifecycle method declarations in a
  small ordered typed catalog. Compile-time negative controls reject reordered
  methods and changed ownership despite an unchanged class size. Its partial
  test schema is not a qualified C++ profile or published signature.
- `compile_probes.py` records all four native `IBaseSystem` declarations and
  five real HLTV concrete-to-interface/lifecycle conversions. The legacy factory
  cast is emitted beside the compiler's real base conversion. No raw pointer or
  member pointer representation is inspected or assumed.
- `interface_probes.py` preprocesses four actual owning TUs to retain header
  resolution, and records selected IVGui/ISurface AST/vtable facts under actual
  owner flags. Matching primary indices are controls, not a complete acceptance
  claim for their transitive exchanged types.

The accompanying artifact report proposes production work for owner review.
MSVC layout/slot qualification, complete catalog coverage, injected negative
loaders, callback ordering, dependency/bootstrap binding and runtime validation
remain future implementation work.
