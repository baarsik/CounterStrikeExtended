# Apple engine heap boundary

This component exercises the real `quakeparms_t` from `engine/host.h`, the complete
current SW/HW `Sys_InitMemory`/`Sys_ShutdownMemory` bodies, the original Win32 bodies
with actual Windows SDK types, and `common/NativeEngineMemory.h`. It does not load
an engine/game/provider or establish a whole-runtime compatibility result.

The expected 116 healthy-case byte values come from six actual MSVC profiles of
the original Windows functions. They are fixed in the immutable reference packet,
not calculated with the candidate planner. The original Mac complete-function
fixture first exposed 15 dedicated-policy disagreements, wrong sysconf names, and
the SW Windows-type dependency. Negative/overflowing legacy shifts are never run.

Run from the repository root, using an explicit reference packet:

```sh
python3 tests/native/engine_memory/run_mac_matrix.py \
  --art /absolute/new-output/mac \
  --reference /absolute/engine-memory-boundary-v1 \
  --contexts /absolute/engine-memory-boundary-design-v1/ACTUAL-MAC-CONTEXTS.json
python3 tests/native/engine_memory/run_mutations.py \
  --art /absolute/new-output/mutations \
  --stage /absolute/new-output/mac/source
```

The final packet is under
`/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/engine-memory-boundary-v1`.
Its source/stage manifests pin the reference inputs and all compiler identities.
`run_windows_component.py` is a headless VM script with explicit stage, output,
protected-header and owned-work directory arguments; its exact invocation and
compiler paths/hashes are recorded there. It builds/runs x86, x64 and ARM64 R/D
portable tests and current Windows bodies, compares every healthy row with the
original SDK reference, and compares both owners' selected Windows tokens. Its
extra failure fixture imports the **original** Windows functions and captures
24 known-low-RAM order cases per owner plus allocation failure. Actual SDK
signatures are checked before binding controlled primitives.

The Apple body fixture controls only external query, allocation-failure and fatal
boundaries. Healthy allocations use real malloc/free, serially, with one live
arena of at most 128MiB. The read-only named SDK RAM query must report capacity
above 1GiB plus that arena before the fixture allocates. This is a physical-capacity
guard, not an available-memory/pressure guarantee. The Windows healthy oracle
separately checks real available memory for the same reserve before each process.

The selected Apple behavior preserves automatic 14/40/128MiB sizing and the
known-RAM 15MiB fatal check before minimum/dedicated overrides. Unavailable,
nonpositive or overflowing page products have explicit unknown statuses and use
the approved Windows zero-RAM fallback, client 128/dedicated 40, before minimum
override. Explicit configured values below 14MiB remain automatic. Native heap CLI
parsing preserves the measured ASCII decimal-prefix domain but rejects negative
or overflowing signed32-byte requests transactionally. Missing values retain the
existing ignore behavior. Accepted KiB are multiplied only after a checked bound.

The C++14 R/D/ASan+UBSan controls check the meaningful state, value, destination
bytes and call order. Both SDK queries are checked by their actual names and
signature, with page counts above 32bits, nonpositive results and product overflow.
All eight isolated source mutations must compile and then fail behaviorally.
Apple LeakSanitizer is unavailable on this host; that initial diagnostic is kept,
and the successful ASan/UBSan runs explicitly disable leak detection. No LSan or
general leak-proof claim is made. Compiler FP-option diagnostic and SDK-name
fixture failures are also retained, rather than hidden.

The four exact owning-context **extracted** function compiles preserve source FP/EH
flags and succeed. Full owning translation units still fail first at unconditional
Windows headers; those failures are recorded separately and not suppressed or
qualified away. `/EHa` in the Windows reference and C++ exceptions in the controlled
Mac fatal binding are not a hardware-fault/unwind equivalence claim. Windows and
other POSIX selected function tokens are unchanged. `host_parms`, existing headers,
allocator contents, other POSIX behavior and non-heap functions are not repaired
by this component. Parent/root owns manifest integration and full-runtime gates.
