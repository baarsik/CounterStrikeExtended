This fixture qualifies the scoped native representation boundaries in the actual
filesystem module. It owns no ABI/header/manifest changes. The full module and
existing VFS regression driver are built by adding the existing
`tests/native/filesystem` project: its retained-manifest evaluator supplies the
source inventory. There is no copied production module list.

The actual owning generated compiler rows separately compile all four changed
TUs in Release/Debug and ASan+UBSan. Scalar clock tests use the whole actual dbg.cpp
TU with typed gettimeofday binding. Path diagnostics compile the whole actual
pathmatch.cpp TU plus a fixture-only bridge to its internal Descend routine.
Module value tests link the real manifest-built module and call its production
methods, including actual PACK loading, shared views and public VFS size queries.

The Windows reference runs actual extracted low-level method bodies (only their
private class qualifier is adapted), real CRT/NTFS calls and actual
FileTimeToString body for valid inputs. It records compiler, function-owning CRT
image SHA256/loaded PE machine and actual probe binary identities for x86/x64/ARM64
Release/Debug. Assertions from failed production fseek are recorded by a typed
fixture binding; no modal assertion dialog is opened. Ctime invalid-domain results
are queried directly without executing the subsequent unsafe legacy strncpy.

The measured contract is deliberately legacy32:

- fseek's int64 argument is converted to the signed low32 word, including wrap:
  2^32 seeks zero and -2147483649 becomes 2147483647. The native implementation
  encodes that word explicitly, with no implementation-defined signed overflow.
- ftell positions above INT_MAX fail with -1/EINVAL, preserving other underlying
  native failures such as ESPIPE. Cursor setup uses zero-length files and never
  extends their EOF or writes a gap.
- Windows stat rejects sizes above INT_MAX with EOVERFLOW and zeroes the output,
  including poisoned-buffer controls. Native stat/fast-size check representability
  instead of truncating off_t. Missing-file output/error are checked too.
- Timestamp fields retain signed low32 bit encodings. Both loose and actual tiny
  PACK views exercise endpoints and post2038 values; disk records/signatures stay
  unchanged. Public name-size/existence and the retained Open-with-zero-metadata
  behavior after a reported stat failure are checked with real sparse files.
- FileTimeToString matches 12 actual Win32 UTC-normalized byte goldens, including
  NUL/padding/truncation. Native unsupported dates/null ctime fail with an empty
  string/EINVAL. This intentionally avoids the legacy Windows null-source failure;
  it does not invent a wider time API or date tolerance.
- The private performance-clock epoch uses native timeval seconds, retaining its
  first-call fraction and zero-epoch sentinel quirk. Exact binary64 deltas after
  2038 and backward movement are checked, without changing the clock algorithm.
- Path diagnostic levels use their actual unsigned-int format and every output
  byte is checked against an independently enumerated owned directory. Warning
  text is passed as data to stdout and the formally variadic format callback;
  percent-n paths and 4095-byte truncation are exact byte tests.

Windows-selected production tokens remain unchanged. Native unconditional warning
calls use the declared QUIET(0) threshold locally in the two CPPs. The legacy -1
macro casts outside the enum's declared 0..3 range and produced a retained UBSan
RED; no enum/header/ABI change or suppression was used. All internal uses were
traced, no retained setter/config caller stores a negative level, and all four
valid configured levels plus existing null-handle API paths are tested. Arbitrary
caller-constructed negative enum arguments remain outside the native contract.

Metadata time meaning is separate from integer/format encoding: Windows st_ctime
is creation time; POSIX st_ctime is status-change time. Empirical Windows stat also
changes when its CRT TZ is altered. Reference metadata is therefore collected
before TZ normalization; only formatter controls use UTC0. No birthtime, OS clock
or production timezone behavior was rewritten.

Windows high logical sizes require successful FSCTL_SET_SPARSE plus verified
FILE_ATTRIBUTE_SPARSE_FILE and allocation queries before/after, with at most 1 MiB
allocated and at least 16 GiB free. No gap is filled or written and every temporary
file is deleted. The initial 43 GiB guard receipt is retained; root clarified that
43 GiB was observed headroom, then explicitly approved this bounded 16 GiB mode.
Sparse setup/query failure has no allocation fallback. Mac sparse files similarly
check st_blocks allocation at most 1 MiB; their data lives only in the owned build
root. No runtime producer source/build is modified.

Mac example (actual compiler database and frozen Win32 format log required):

```sh
cmake -S tests/native/filesystem_platform -B build/filesystem-platform-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_FS_PLATFORM_DATABASE=/absolute/path/to/Release/compile_commands.json \
  -DCSE_FS_REFERENCE_LOG=/absolute/path/to/frozen/x86-Release/probe.log \
  -DCSE_FILESYSTEM_CASE_ROOT=/absolute/owned/case-sensitive-volume/release
cmake --build build/filesystem-platform-release --parallel 3
ctest --test-dir build/filesystem-platform-release --output-on-failure -V
```

Repeat Debug with its actual database; add CSE_FS_PLATFORM_SANITIZERS=ON for
ASan+UBSan. A fresh owned case-sensitive APFS sparse image qualifies the existing
case regression. Proven attachment uses hdiutil -nobrowse -noautoopen -mountpoint;
this host no longer accepts the old -nospotlight flag. The fixture creates its own
.metadata_never_index marker and detaches the image after tests. No global system
indexing/login/signing settings change.

The inherited filesystem harness omits repeated image reload only when the module
is instrumented on Apple, retaining its earlier sanitizer-global-registration
qualification split. Release/Debug still run 64 actual reloads; instrumented API and
mount/unmount cases remain enabled. No sanitizer suppression or new claim about
that inherited loader limitation is made here. No Linux/full-game acceptance,
arbitrary high PK64 file support, all metadata semantics or external consumer
compatibility follows from this bounded component gate. No game, Steam, network,
UI or desktop operation is executed.
