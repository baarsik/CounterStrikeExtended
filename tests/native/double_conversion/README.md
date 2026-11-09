# Retained binary64 to signed32 conversion

The new header converts IEEE binary64 directly, without a binary32 detour or an
out-of-range scalar C++ cast. Its scalar decoder and x86 SSE2 intrinsic path must
match every encoded result of the measured retained Win32 conversion. This is a
component gate; it does not wire or qualify a VGUI backend or complete runtime.

`extract_reference.py` copies the actual `CSystem::GetTimeMillis` body unchanged
from `vgui2/src/System.cpp`, pins its source/manifest hashes, and verifies the
owning C++14, `/fp:precise`, and exception profile. A minimal class supplies only
controlled double `GetCurrentTime` values. It is an arithmetic-body fixture, not
the production provider or a replacement class ABI.

Collect the reference first in an x86 MSVC environment:

```sh
cmake -S tests/native/double_conversion -B build-double-reference -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSE_DOUBLE_REFERENCE_ONLY=ON
cmake --build build-double-reference --parallel 2
build-double-reference/double_reference.exe /path/to/new/win32-prefix
```

Repeat Debug and compare all three output files before freezing the golden.
The prefix produces `-direct.bin` and `-method.bin` little-endian encoded signed32
results and `-scaled.bin` binary64 values captured after the method's scaling.
There are 35 explicit bit patterns plus 1,000,000 seeded binary64 patterns.
`golden_manifest.json` records the qualified Win32 corpus identities.

Run the helper gate with that frozen prefix:

```sh
cmake -S tests/native/double_conversion -B build-double -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_DOUBLE_GOLDEN_PREFIX=/path/to/win32-prefix
cmake --build build-double --parallel 2
ctest --test-dir build-double --output-on-failure
```

`CSE_DOUBLE_SANITIZERS=ON` adds ASan/UBSan on Clang. Without a golden prefix, only
the boundary packet and floating-state probes run; this is not the complete
corpus gate. `CSE_DOUBLE_NATIVE_CAST_BASELINE=ON` demonstrates the original native
cast mismatch before using the helper and intentionally includes out-of-range
casts. It is a negative reference mode, not a sanitizer gate.

The full comparison checks scalar and native helpers independently against every
Win32 encoded result, including conversion of captured method-scaled binary64
values. It does not infer that arbitrary native compiler casts match Win32.

The separate FP probe records native control/status registers, and x87 CRT state
where available, under four rounding modes with clear and seeded sticky flags.
It verifies unchanged control registers, preservation of preexisting sticky
flags, and retained helper values. The scalar path does not recreate hardware
invalid/inexact sticky effects. Value equality is not full floating-machine-state
equivalence. Tests use masked exceptions and do not qualify unmasked traps.

No existing producer, deadline arithmetic, manifests, default target, provider,
game, or desktop behavior is changed by this standalone project.
