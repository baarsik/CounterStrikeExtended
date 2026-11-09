# Unselected AppCold metadata and admission

This is a separate dependency-edge catalog: Runtime10, Descriptor288 and the
16-table envelope are unchanged. The AppCold edge has its own role/profile/App
families. Never concatenate it with another edge's colliding role/profile IDs.
No production target, loader, App/font/Surface factory or readiness flag selects
this code. `metadata_ready` reports compiler-generated data availability only;
all publisher/service/cold-load/UI readiness remains false.

The serialized record is exactly352 bytes, little endian:32-byte framing,
32-byte compiler/source provenance digest and the explicit288-byte Descriptor.
No pointer/native padding, export getter or whole-image digest is serialized.
The publisher's array is constant-initialized. A guarded Clang address-sanitizer
attribute preserves that exact on-disk array size; without it ASan adds a64-byte
redzone and the strict reader correctly rejects its416-byte section. All parser,
constructor and owner code stays instrumented. This layout exemption says nothing
about lifetime/readiness. Exact owning recipes additionally force the EXISTING
Steam module initializer into hw/sw publisher objects; their symbol/IR/command
receipts identify that inherited initialization separately. Data-only helper
images demonstrate no metadata initializer or metadata getter export.

`--metadata` requires fresh actual hw_engine + swds vgui_intwrap.cpp and VGUI
common App.cpp compiler-database contexts, with their actual public/private App
headers. It refuses fixture-only/copied/forced-alternate declarations and ANY
injected generated AppCold header. It never includes its output into declaring
probes. Inputs, compiler identities, options, environment and transitive headers
are provenance. Actual types, fields, layouts, slots, enums and constructor facts
provide ABI signatures. No cached/manual fallback survives a rejected refresh.

`tools/vgui1_app_cold_manifest.py` separately binds the finished immutable image's
size/SHA256/LC_UUID and serialized record digest to its fresh compiler receipt.
This external produced manifest avoids an image self-hash cycle. It is not code
signing, malicious-filesystem defense or a loader-namespace guarantee. A future
host must validate its own trusted manifest parsing/deployment policy.

Manifest publication pins the exact receipt and observations bytes parsed and
validated initially. Validation checks that those files still contain the captured
bytes, and publication requires an identical final validated snapshot/reference.
The manifest emits the captured `receipt_sha256` and additive
`observations_sha256`; it never substitutes a later unbound receipt-path hash.
This detects the controlled refresh interleavings, not arbitrary concurrent or
ABA mutation. Immutable staging and the existing namespace/lifetime prerequisites
remain mandatory; there is no locking or general race-free/security guarantee.

The reader uses bounded spans or streaming checked `pread`, fixed local storage,
SHA256 from the selected SDK/libSystem, no heap/EH/RTTI, and never `dlopen` or calls
image exports. It accepts only thin little-endian native64 Mach-O, a single
regular metadata section and UUID, known version/CPU/table/role domains, bounded
load commands/sections/dependency names, and exact external pins. Limits are
512MiB image/1MiB load commands/4096 commands/1024 sections/32 strong dependencies/
1024-byte paths. Refusal leaves output untouched. Only an exact ABSOLUTE strong
provider dependency matching the canonical pinned provider can return
`PinnedPairCompatible`. `@rpath` is `ResolutionUnsupported`, never guessed.

The actual cold fixture compiles WHOLE original vgui_intwrap.cpp and its real
static CDummyApp, whole common/posix App.cpp, Scheme, Font owner/adapters, Cursor,
DefaultCursor, Bitmap, Image and Color. Consumer and provider use their actual
distinct declaring headers. Fixture compiler profiles are explicit controlled
C++14 bindings; the separate compiler-replay gates use actual producing recipes.
Unrelated methods use typed complete real headers and abort if called. There is
no substitute App/Scheme/Font/Cursor/Bitmap object and no real font/provider/clock/
Surface/SDL/window/renderer/UI/device/network use. The cold clock is an injected
constant with an independent shared counter; provider lookup aborts if reached.

Before explicit load, mismatch must leave the counter0 and both images unmapped.
A match constructs App664, four named fonts with IDs100..103 and PendingResources,
and thirteen actual cursor/bitmap/image objects (none0/0, ordinary16x16 cross/
hotspot8/8). It checks the real borrowed App instance, signed32 tick1234/interval50,
exact pending font status and actual ordinary cursor pixels. The constructor
leaves the Scheme user slots uninitialized; the fixture NEVER reads them. A
preliminary invalid fixture enumerated sf_user and failed; that failure is not
production evidence. Final enumeration is explicitly the four constructed fonts
and thirteen constructed cursors only.

The host checks loaded symbol paths/LC_UUID after construction and unchanged
files. Unique absolute install names + absence from dyld before load are fixture
preconditions. Production must resolve global/preloaded namespace collisions
BEFORE admitting constructors, and retain every borrowed App/default resource
until safe retirement. Post-load identity alone cannot undo constructor effects.

Cleanup is FIXTURE ONLY: the known pristine default Scheme allocations are
explicitly destroyed with their complete actual types; typed member access frees
CursorBitmap RGBA and the two empty App Dar arrays; the borrowed static App pointer
is cleared before normal release. Production can borrow replacement Scheme/fonts/
cursors, has no equivalent destructor contract, and MUST NOT reuse this cleanup.
Normal owned root release must actually unmap root and provider. No permanent
release-failure/uncooperative client safety, real services, actual module staging,
full native VGUI selection or general repeated-ASan-dlopen claim follows.

Run the auditable matrix with fresh output and actual context files:

```
python3 tests/native/vgui1_app_cold_admission/run_matrix.py \
  --contexts /absolute/context-directory \
  --output /absolute/new-receipts --build-prefix unique-app-cold
```

Context files are `release-actual-contexts.json` and `debug-actual-contexts.json`.
The unselected `cmake/VGui1AppColdMetadata.cmake` helper has separate data-only
controls in `helper/`. Parent root owns any eventual production membership and
loader/host-owner integration. No App(bool) constructor or other public VGUI class
ABI catalog is added by this edge.
