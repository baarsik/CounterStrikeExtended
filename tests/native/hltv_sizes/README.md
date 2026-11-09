# HLTV native counts and word access

The fixture compiles complete affected production DirectorCmd methods, the entire
InfoString implementation, the complete CD-key count/copy method, and MakeHull0
with its native index helper. Real class declarations back DirectorCmd,
InfoString and BitBuffer. The actual BSP plane/node/clip/hull declarations back
an explicitly owned MakeHull0 class/system context; this is not a BSPModel class
layout or complete loader qualification. Production source bytes are extracted
without rewriting their algorithms.

Production boundaries:

- Director payload size stays int. Native string counts are checked before any
  addition/narrowing/allocation, retaining fixed 29-byte message headers, 4-byte
  sound volumes and terminating NULs. Invalid native counts clear/free the owned
  command, set sizeError and make a stream read return false. Internal payloads
  above 254 bytes remain stored, while the existing one-byte transmit gate still
  declines to send them. No message layout or protocol value changes.
- InfoString capacity remains int behind its unsigned API. Constructors reject
  native counts/minima above INT_MAX; SetMaxSize rejects such values before
  allocation and preserves the existing object. The append capacity check now
  compares native counts without narrowing a potentially overflowing sum.
- The reported shared md5.cpp site is strlen of a CD key, not fread. Its native
  length remains native until the existing 1..255-byte key limit proves the
  explicit unsigned MD5_Update count safe. Buffer and hash format widths stay
  unchanged. Empty/256-byte keys remain rejected.
- MakeHull0 retains the 8-byte clip record and signed int planenum. Its checked
  native address index must match the actual 20-byte plane stride, lie at/above
  the plane base, and be less than the positive int model.numplanes. A rejected
  plane releases/resets the just-created hull0 allocation before Errorf/return.
  LoadPlanes checks its 20-byte disk stride but has no MAX_MAP_PLANES validation;
  LoadNodes constructs plane pointers directly. General malformed-lump bounds,
  allocation-failure policies and existing short child-index casts remain outside
  this repair.

BitBuffer now moves exact uint32_t words through memcpy. Stream word accesses
copy min(4, logical bytes remaining), zero-fill unavailable read bytes and store
only available bytes. Zero remaining bytes cause no memcpy call. Raw bulk group
loads/stores still copy four bytes: the existing iSize/dword grouping proves
those complete input/output groups exist. Masks, shifts, byte/bit cursor updates,
endian branches, OR semantics and existing request/error decisions remain intact;
this does not introduce a general bit-length validation refactor.

Resize allocates logical size + 4 padding bytes, but SetBuffer and the external
constructor receive no padding/physical-capacity argument. Actual external users
include exact char[4037] buffers in NetChannel::TransmitOutgoing/OutOfBandPrintf,
and World frame/delta entity spans. Requiring extra caller padding would exclude
existing API inputs. An isolated old ASan receipt demonstrates a four-byte read
from a real one-byte allocation; old UBSan receipts separately expose unaligned
word loads/stores.

The 18 cases include fixed sound/banner/stufftext/message wire goldens and read
round trips, the 254/255 transmit fence, native storage/INT_MAX/SIZE_MAX fences,
InfoString exact fits and preserved resize bytes, exact CD-key bytes forwarded
to hashing, high-address hull bytes and rejected plane indices. Controlled
strlen bindings inject 4-GiB-plus counts without allocating multi-gigabyte
strings; allocation bindings record whether rejected counts reach allocation.
Those injected counts are boundary fault cases, not claims that such strings
were allocated or scanned.

The bit corpus independently checks 528 combinations: both bit orders, widths
0..32 and bit offsets 0..7, with unaligned/near-end addresses, payload bytes,
cursor/error state, padding and canaries. Its 25,344-byte record matches the old
reference byte for byte. Separate unaligned bulk input/output and exact one-byte
external spans verify bounds; a zero-remaining no-op write verifies that memcpy
never receives an unavailable span. These are real allocated extents, with
padded legacy storage kept separate from exact external storage.

The entire actual BitBuffer implementation is compiled. Its three constructors now
initialize data members while preserving the compiler-owned virtual table; the
prior whole-object memset fails the retained constructor/destruction regression.
Default, owned and external construction use virtual destruction through another
translation unit. The CD-key method
uses owned hash-call bindings which record exact input bytes and return a fixed
digest receipt; **the MD5 algorithm is not qualified here**. Its unchanged
sizeof(pointer) final zeroization and deprecated sprintf remain open for the
separate native crypto audit. No compiler diagnostic or sanitizer is suppressed.
Complete BSP/Director translation units, real file I/O, broadcasts and gameplay
remain unqualified.

From a matching x86/x64/ARM64 MSVC prompt with Python, Ninja and the SDK:

```powershell
cmake -S tests/native/hltv_sizes -B C:/CI/CSE-HltvSizes/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64
cmake --build C:/CI/CSE-HltvSizes/x64-Release --parallel
ctest --test-dir C:/CI/CSE-HltvSizes/x64-Release --output-on-failure -V
$env:CSE_HLTV_BIT_CORPUS = "C:/CI/CSE-HltvSizes/x64-Release/bit-corpus.bin"
& C:/CI/CSE-HltvSizes/x64-Release/hltv_size_tests.exe bit-words
```

Use matching compiler prompts and separate directories for x86/x64/ARM64 and
Release/Debug. The expected corpus SHA256 is
`d35f7784f36f5e81389a044b69a77725720fc7a1658781df60f5721fa940ffe6`.
All production changes are in existing files; no new manifest membership is
required. Windows/full runtime qualification is separate.

macOS ARM64 uses separate build/hltv-sizes-macos-arm64-{release,debug,asan}
directories and -DCSE_TARGET_ARCH=arm64; Debug adds
-DCSE_HLTV_SIZE_SANITIZERS=ON for ASan/UBSan. An archived SDK-shaped original
snapshot can be selected with -DCSE_HLTV_SIZE_SOURCE_DIR=... and
-DCSE_HLTV_SIZE_LEGACY=ON. Its narrowing warnings remain visible, the original
count/plane cases are expected RED, and ASan/UBSan must expose the old word
accesses. Win32 skips nonrepresentable 4-GiB-plus native length injections while
still running the signed-size fences and actual byte/reference paths.

The three `hltv-demoplayer-constructor-*` cases also compile the distinct private
DemoPlayer BitBuffer implementation and its own header. They check default, owned
and external field state, then destroy through a base pointer in a separate
translation unit. This catches constructors that erase the vtable; these cases
do not qualify the private implementation's complete bit-stream behavior.
