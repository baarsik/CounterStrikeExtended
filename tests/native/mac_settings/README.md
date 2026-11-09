# Private native Mac settings component

This is an unwired component. Only `common/mac_settings_store.{h,cpp}` are production candidates.
The header is C++14 and contains no CoreFoundation types, App fields, virtual methods or callbacks.
Runtime use requires source selection, CoreFoundation linking and the Mac App adapter.

The product domain is exactly `org.counterstrikeextended.settings`, current user / any host. Release and
Debug intentionally share it. No Steam, global preferences, browser-profile or cwd setting is redirected.
No test in CTest calls CFPreferences. Real persistent fixtures are built but require a separate reviewed run.

## Byte and type contract

Each preference key has ONE native CFDictionary value with exactly these three fields:

| Field | Native type | Contract |
|---|---|---|
| `version` | integer CFNumber | exactly 1; floating-point, bool and unknown versions fail |
| `kind` | integer CFNumber | 0 = Binary, 1 = String; never inferred from matching bytes |
| `payload` | CFData | original opaque bytes; String includes its terminating NUL |

This is a native property-list value handled entirely by CFPreferences, with no file parser or added library.
Raw unversioned CFData is rejected. The earlier development representation was never persisted.

`WriteBytes` and `WriteInt32` create Binary values. Integer encoding is exactly four little-endian bytes.
`WriteString` creates a String, accepts strict UTF8, and includes NUL. Invalid UTF8 fails before Set/Sync.
Queries do not coerce Binary to text or parse decimal integers. A short Binary read into an existing integer
changes only its low bytes; zero-length Binary leaves it unchanged. Oversized or missing values return false.

String queries preserve measured `RegQueryValueExA` behavior in the **ACP65001 reference profile**:
when caller capacity fits the UTF16LE storage extent, write that storage including its terminator and then
overwrite its prefix with the original UTF8 bytes including NUL. When only the UTF8 payload fits, copy only
that payload. Thus a successful String read may modify more bytes than `requiredBytes`, always within capacity.
For an existing integer `0x12345678`: empty String yields `0x12340000`, `A` yields `0x00000041`, `AB` yields
`0x12004241`, and `ABC` yields `0x00434241`. Binary `41 00` instead yields `0x12340041`.

A null destination is a size query. Required size is the original payload extent, not UTF16 staging size.
An undersized query reports required size and leaves the caller unchanged; Windows defines the contents of
ERROR_MORE_DATA buffers as undefined. Missing, malformed, invalid-key and synchronization failures report
zero required bytes and leave the caller unchanged. No registry getter has an offset argument. Caller key and
String inputs must be valid NUL-terminated allocations; byte buffers must cover the supplied extents.
Lengths/capacities above DWORD range are rejected before data access; native size_t/CFIndex staging arithmetic
is checked at compile time. No partial value is copied on failure.

SetValue is followed by Synchronize, and only successful synchronization acknowledges a write. SetValue has
no independent error result. A sync failure returns false but does not promise rollback of CF's pending cache.
Reads synchronize before CopyValue so that fresh-process/concurrent-key behavior can be tested honestly.
Each owned CF object is released in the call. Framework-owned preference caching can retain copied CF values;
it retains no module callback or C++ object. The helper has no logging, global destructor or persisted callback.

## Explicit native encoding decision

The native policy uses strict UTF8 for native keys AND String values after the real Windows diagnostics. The selected
native String contract matches the process-local UTF8-activeCodePage reference, not arbitrary default-1252
behavior. Binary remains byte-preserving. The retained-source search found only App declarations/platform
implementations and **zero registry callers**, no actual legacy-encoded String inputs or external Steam/OS keys.
That is bounded source evidence; it is not a claim about unknown external consumers.

Keys use strict UTF8, canonical NFC, Unicode case folding with explicit `en_US_POSIX`, then NFC. This preserves
ASCII case-insensitive full-key identity and canonical Unicode equivalence. Literal backslash and slash remain
distinct CF key characters; no filesystem/path traversal or separator rewriting occurs. Empty or trailing-
backslash keys fail like retained SplitRegistryName. Case-fold tables come from the running OS CoreFoundation;
this is an explicit native Unicode policy, not Windows ANSI key/codepage equivalence.

The first Windows expectation was wrong: on ACP1252, querying an `A` REG_SZ into a four-byte integer overwrites
four bytes although returned size is two. The failed v1 and subsequent diagnostics are retained in ART.
A process-local UTF8 manifest changes only the owned diagnostic executable; no system locale/registry policy
was changed. Ten controlled inputs, 170 capacities, ten W-storage captures and ten actual App integer queries
were observed in each of ACP1252 and ACP65001. The checked-in vectors contain the latter reference's exact bytes.
See [Microsoft's API contract](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexa)
and [process-local UTF8 documentation](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page).

## Nonpersistent checks

```sh
cmake -S tests/native/mac_settings -B build/mac-settings-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-settings-release --parallel 2
ctest --test-dir build/mac-settings-release --verbose
cmake -S tests/native/mac_settings -B build/mac-settings-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-settings-debug --parallel 2
ctest --test-dir build/mac-settings-debug --verbose
cmake -S tests/native/mac_settings -B build/mac-settings-san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_MAC_SETTINGS_SANITIZERS=ON
cmake --build build/mac-settings-san --parallel 2
ctest --test-dir build/mac-settings-san --verbose
```

The model includes the actual implementation, real CFData/CFDictionary objects and injected typed Copy/Set/Sync
operations. Compile-time blockers replace all three CFPreferences primitives, count attempted persistent calls,
and fail the fixture if any are attempted. Its linked image must contain no undefined CFPreferences symbol.
Coverage includes 170 real Windows UTF8 capacity vectors, String/Binary identity, malformed/version/type paths,
invalid UTF8, raw bytes/NULs, read/write sync and copy failure, scoped retain counts, DWORD bounds, and source/
guarded destination addresses above 4GiB. The public header also builds/links without CoreFoundation.
The standalone Win32 CMake branch extracts the actual retained App methods and string-copy helper verbatim;
its single CTest creates/deletes only a fresh `HKCU\Software\CounterStrikeExtendedFixture\MacSettings-*` key.

Current receipts target macOS 14 but run on host macOS 27.0.1, Apple clang 21.0.0, ARM64. They do not establish
actual macOS 14 runtime, full VGUI1, module integration, game behavior or persistent preference acceptance.

## Explicit persistent fixture

`mac_settings_persistent` is compiled for Release/Debug but is deliberately absent from CTest. Its guard accepts
only `org.counterstrikeextended.fixture.settings.` followed by exactly 32 lowercase hex digits. It rejects the
product domain before any preferences call. No product-domain API is called by this executable.

These commands explicitly execute the isolated persistent fixture; the component results above do not cover them:

```sh
python3 tests/native/mac_settings/run_persistent_fixture.py --executable "$PWD/build/mac-settings-release/mac_settings_persistent" --evidence /Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/mac-settings-persistent-release-v1 --run-reviewed-owned-preferences-fixture
python3 tests/native/mac_settings/run_persistent_fixture.py --executable "$PWD/build/mac-settings-debug/mac_settings_persistent" --evidence /Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/mac-settings-persistent-debug-v1 --run-reviewed-owned-preferences-fixture
```

Each run generates a fresh UUID domain, records executable hash and child PIDs, writes controlled values,
restarts a reader, runs two concurrent writers for 64 distinct keys each, verifies all keys in another process,
and removes every key from ONLY that unique fixture domain in finally, with a fresh-process empty-domain check.
Children are bounded and reaped before cleanup. Failures and forced cleanup are reported. Output contains
operation/status/counts, never setting payloads. CF may retain an empty domain container; no plist is unlinked
behind cfprefsd. This qualifies sync acknowledgements and process restart, not power-loss durability or real OS
permission/write-failure behavior. Those failures are currently injected/model evidence only.
