# Complete CS smoke boundary fixture

The only production change is the Apple-only smokeType0..4 early check and
explicit outer-default return in EV_HLDM_CreateSmoke. Unsupported Apple values
return before queue/RNG/model/sprite effects. Supported bodies and Windows
selected source remain unchanged. This is CPU/data component qualification,
not model-resource loading, temp-entity engine execution, rendered smoke or
full gameplay acceptance.

`extract.py` hashes complete actual CreateSmoke and all four actual callback
bodies. It preserves existing MSVC4701 pragma framing. Real SDK Queue/Element,
Vector, TEMPENTITY and callback-table types are used. Only the actual-declared
float CHud::m_flTime value is projected; no fake CHud object/layout is constructed.
The old reference is never called with invalid types. Its Clang diagnostic is
emitted in an explicitly reference-only build; no production flag changes or
model-warning suppression are added. Candidate/harness use Werror, with only
existing owning-recipe legacy header diagnostic policies inherited. Windows
uses the retained CRT deprecation definition and existing source pragmas.

```
python3 tests/native/cs_smoke_contract/run_macos.py \
  --output /absolute/new-receipts --build-prefix unique-smoke-fixture
python3 tests/native/cs_smoke_contract/replay_compile.py \
  --database-root /absolute/artifact-root --output /absolute/new-owner-receipts
python3 tests/native/cs_smoke_contract/mutants.py --output /absolute/new-mutants
```

The corpus has204 valid complete-body cases and8 Apple invalid/no-effect cases.
It compares logical fields and ordered API traces, never padding or unused
uninitialized members. It covers all resource selectors, lookup/allocator
failures, full-queue type0 behavior, repeated/changing cvar reads, origin equality
masks, retained speed/scale/rate ranges, callback thresholds and shared wind across
two actual owned entities. Callback bodies and Queue ownership remain unchanged.

VM runner `windows_matrix.py` uses a fresh owned workspace, compiler environments
held only in memory, x86/x64/ARM64 R/D PE identity checks and a4GiB disk floor.
No UI/provider/game/network runs. Preserve stdout/stderr separately when collecting
raw values; merged CRT streams can interleave the final status message into a row.
Compiler/source/binary hashes and exact controls are in the sealed artifact packet.

The shared dependency utility used by the whole-TU replay requires a
cse_module_abi.h bookkeeping anchor. That extra pinned input is not claimed to
be an actual smoke include or an ABI proof. Owning Windows full-module rebuild
and rendered/gameplay qualification remain separate root gates.
