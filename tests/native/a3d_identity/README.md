This fixture qualifies the private native geometry identity lane. It does not
build or load the A3D COM module, provide an A3D backend, open a sound device, or
qualify game integration, audio presentation, or spatial/geometry feature parity.

`extract_sources.py` compiles the selected current production method bodies
without rewriting them. `production_bodies.inc` has source line directives and
`extracted-bodies.json` records their hashes. Production primitive/command,
reflection/control records and the command-buffer declaration are extracted too.
`fixture_support.h` supplies the declarations needed to isolate those bodies
from Windows COM/device headers. Its material/opening branches abort if reached;
these branches and the complete production class ABI are outside this fixture.

The exercised path is actual face rendering, Tag/Begin/Vertex submission,
command-pool allocation/copy/read, list capture/replay/Clear, source geometry
reflection generation, reflection gain/delay math, short/long sorting, duplicate
suppression, previous-frame slot assignment, polygon copying/deletion, and
caller-owned `InitializeReflectionSlots`. The inherited `GetNextVertex` behavior
of returning the opcode through its tag argument is deliberately asserted.

On native 64-bit builds the fixture reserves a sparse virtual range and commits
only two pages, 4 GiB apart, for two real polygons with identical low 32 address
bits. The production identity keeps the full address and a domain distinct from
public DWORD tags. It retains the old non-owning address-key behavior: command
copies preserve the original key after polygon deletion; a clone has its own
address; exact full-address reuse remains the inherited identity rule. No
recycled 32-bit registry IDs are introduced. Win32 retains the original shared
DWORD domain, record storage, and command allocation size.

Only private native allocations grow: command-buffer storage has a parallel
identity array after the unchanged command array, and each owned reflection
allocation has the unchanged COM record first plus an identity. Frame recycling
frees the complete reflection allocation through that first-member address.
External records passed to `InitializeReflectionSlots` are never adopted, wrapped,
or used as private identity-bearing nodes. There is no global identity registry,
shared-module owner, serialization format, or new public COM method.

From the repository root on macOS:

```sh
cmake -S tests/native/a3d_identity -B build/a3d-identity-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/a3d-identity-release
ctest --test-dir build/a3d-identity-release --output-on-failure
```

Use a separate build directory with `Debug`; add `-DCSE_A3D_SANITIZERS=ON` for
ASan/UBSan. On Windows use a matching x86, x64 or native ARM64 MSVC command prompt,
omit the two OSX options, and set `CSE_TARGET_ARCH` accordingly. Native Windows
fixture execution and full-module compilation are separate qualification gates.

`CSE_A3D_SOURCE` can select a preserved source directory. `CSE_A3D_REFERENCE=ON`
uses Clang's MS pointer-cast extension only to execute the original truncating
cast for the reference RED. The strict production build has no cast warning
suppression. The original fails the high-address collision, replacement,
public-domain, recorded-list and slot-history assertions; its duplicate/query
and unaffected public/copy/slot-initialization assertions pass.

The unowned `IA3dDal::InitializeEx(DWORD device, ...)` provider contract is
unchanged and remains a native blocker. This step does not supply or substitute
a native provider. Existing command-buffer/reflection allocation-failure handling
and source-destructor reflection cleanup remain inherited; this fixture uses
valid allocations and the existing two-frame recycling path for cleanup.
