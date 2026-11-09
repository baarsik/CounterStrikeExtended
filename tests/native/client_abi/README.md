# Owned engine/client C ABI component

This fixture covers the owned engine ↔ Counter-Strike/Half-Life client C tables.
It does not qualify gameplay, the complete engine/client translation units, other
DLL roles, C++ interface factories, or compatibility with unrecompiled binaries.
Runtime ABI version 10 requires rebuilding the owned modules, including Win32.

## Contract and integration

`tools/client_module_layouts.py` generates the retained declarations' compiled
layout expressions. The current catalog contains nine bounded entries, with 66
record contracts and 1,268 exposed fields. Every field contributes its identity,
offset, width, alignment and resolved function prototype where applicable.
Nested callback families and full array/by-value record layouts are included.
The compiler resolves typedefs and calling conventions before prototype hashing;
equal function-pointer sizes alone cannot establish compatibility.

The C profile records the compiler/type-spelling producer and ABI-relevant scalar
properties. It deliberately permits the retained Half-Life `/MT[d]` and
engine/Counter-Strike `/MD[d]` policies. This boundary transfers no STL objects or
CRT allocation ownership: storage stays with its owner or uses the owner's
release callback. It does not impose equality of iterator, RTTI, exception,
optimization or language-version settings unrelated to the exchanged types.

There are two individual-pointer prefix views. `PlayerInfo(index)` returns one
engine-owned record; the client does not use its shorter type as an array stride.
Its engine suffix contains the CD-key hash and Steam ID. Counter-Strike's movement
view ends at `PM_PlaybackEventFull`; the engine/Half-Life owner appends three unused
`Ex` callbacks. The contract hashes every exposed prefix field and required extent,
and separately asserts that the owner is large enough. These exceptions do not
apply to arrays, by-value records or exchanged function tables. The fixture writes
through real client views while preserving larger-owner suffix canaries.

`model_t` and `hull_t` layouts are checked. BSP pointees that retained clients do
not dereference remain opaque. The actual studio use of `submodels` as
`cache_user_t[]` has a full `cache_user_t` contract. Raw MDL decoding and renderer
internals remain separate acceptance work.

Engine loading must validate the complete descriptor catalog before querying its
required subset or exchanging tables. This is a pre-callback gate, not a promise
that operating-system module initialization cannot run. The client validates the
engine descriptor before authorizing exactly one legacy `F`, `Initialize` or
studio exchange. Unauthorized legacy entry calls return before touching tables.
Successful initialization is required before studio binding.

`LoadSecureClient` retains the ordinary DLL `F` mode (`fLoadAsDll=1` in the previous
loader): load → descriptor validation → authorized `F` exchange → required-table
validation → state commit. Raw executable-blob mode (`fLoadAsDll=0`) remains a
separate native exclusion. Secure-server/client-integrity data meanings are not
replaced by a stub.

Production integration requires no additional client `.cpp` membership. Each
existing client entry TU includes the implementation once after its complete
type declarations. Retain the new headers and generated header, invoke
`tools.client_module_layouts.generate(ROOT)` from the target generator, and export
these undecorated C names from both client DLLs:

- `CseGetModuleAbi`
- `CseExchangeClientFunctions`
- `CseInitializeClientModule`
- `CseGetClientStudioInterface`

The existing `Exports.h` declarations precede the production definitions and the
implementation's later redeclarations. On MSVC, the first declaration must retain
its `dllexport` annotation; the standalone fixture uses the same export linkage.

## Actual fixture scope

`prepare_fixtures.py` extracts the current production HL/CS `Initialize`, `F` and
studio bodies and the engine's actual loading/validation bodies. Loadable fixture
modules compile those bodies with actual SDK table/record declarations, while
controlling filesystem, event, particle and studio dependencies. These are real
dynamic libraries and OS loader calls, not complete production client images.

The 70 cases include:

- Complete catalog validation, reordered requirements, valid extra entries, and
  rejection of missing descriptors/exports, bad roles/profiles, duplicate or
  reserved unrequested entries, and excessive counts.
- Actual equal-storage declaration mutants: float versus int return values,
  int versus float nested callback arguments, and destination callback pointer
  argument types. Rejection precedes `F` and callback side effects.
- Both actual engine DLL paths: named exports and `F`, including incomplete
  returned tables rejected before committing engine state.
- One-shot legacy-entry guards, valid initialization/studio sequencing, copied
  tables, unchanged CS/HL modifier behavior, owner unload and high-address tables,
  strings, player records, hull pointers and movement state on 64-bit hosts.
- A real cross-module by-value `pmtrace_t`, read-only varargs formatting, typed
  hull callbacks, and preservation of the larger player/movement suffixes.

For an independently built, fully linked production client, run
`client_descriptor_probe <absolute-module-path> --entry-probe`. It checks the
descriptor, rejects bad metadata/unauthorized entries with inaccessible table
addresses, performs one valid high-address `F` exchange and verifies required
callbacks. It deliberately does not call valid `Initialize`, providers or game
code. Passing the controlled fixture does not imply this production-image probe
or game acceptance passed.

## Local commands

```sh
cmake -S tests/native/client_abi -B build/client-abi-local \
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64
cmake --build build/client-abi-local --parallel 2
ctest --test-dir build/client-abi-local --output-on-failure
python3 tools/client_module_layouts.py --check
```

Use `CSE_CLIENT_ABI_SANITIZERS=ON` for local ASan/UBSan. Windows builds use a fresh
native MSVC environment for each x86/x64/ARM64 configuration and `/W4 /WX`.
Fixture CRT policies intentionally differ between HL and CS.
