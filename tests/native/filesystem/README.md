# Native filesystem module fixture

This project compiles the actual `filesystem_stdio` shared module and loads its
`CreateInterface` / `VFileSystem009` boundary. `manifest_sources.py` calls the
owning CMake target for both configurations and the selected target OS, and writes the queried inventory into the
build directory. There is no copied production source list.

The fixture uses only its supplied owned directories and generated literal PACK
files. It checks search-path order and IDs, game/fallback paths, loose/pack
precedence, shared pack-view ownership, read/seek/write/append, preserved duplicate
enumeration, case fallback, resolved paths, native pointer handles, active-find
cleanup, and repeated mount and real module unload/reload. The disk-layout checks
retain PACK's 12-byte header and 64-byte record and PK64's 20-byte header and
128-byte record. PK64 contents and offsets above 2 GiB are not qualified here.

## macOS ARM64

Run from the repository root with the pinned Apple toolchain. A case-sensitive
APFS volume is required for the `case` scenario; the ordinary default APFS volume
does not prove exact spelling and fallback coexistence. For an isolated fixture,
create and mount a disposable sparse image in an owned directory:

```sh
hdiutil create -size 128m -fs 'Case-sensitive APFS' -volname CseFileSystemFixture -type SPARSE /absolute/owned/fixture.sparseimage
mkdir -p build/filesystem-case-sensitive
hdiutil attach -nobrowse -mountpoint "$PWD/build/filesystem-case-sensitive" /absolute/owned/fixture.sparseimage
cmake -S tests/native/filesystem -B build/filesystem-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCSE_FILESYSTEM_CASE_ROOT="$PWD/build/filesystem-case-sensitive/release" -DCSE_FILESYSTEM_LEAK_CHECKS=ON
cmake --build build/filesystem-release --parallel
ctest --test-dir build/filesystem-release --output-on-failure
```

Repeat with `Debug`, a separate build directory, and a separate case-fixture
directory. The optional leak tests invoke Apple's `leaks --atExit` on six actual
module scenarios and require zero leaked bytes. `CSE_FILESYSTEM_RECEIPT_DIR`
selects the full leak-report directory.

For ASan/UBSan, use another Debug directory and add
`-DCSE_FILESYSTEM_SANITIZERS=ON`. This instruments both the actual module and
driver, runs the API fixtures and 64 Mount/Unmount cycles, and omits the real image
reload scenario on Apple. Apple's ASan runtime in the qualified toolchain retains
image-global registrations after actual `dlclose`; the second image load reports
an ODR violation for `g_flDummyFloat`. The original failure is retained in the
qualification artifacts. No sanitizer suppression is applied.

Use a fourth directory with `-DCSE_FILESYSTEM_SANITIZERS=ON
-DCSE_FILESYSTEM_SANITIZE_MODULE=OFF` to qualify 64 actual module unload/reload
cycles with an instrumented driver and explicitly uninstrumented module. That
mode does not qualify the module's accesses under sanitizers. Do not combine
`CSE_FILESYSTEM_LEAK_CHECKS` with sanitizers.

After all runs, detach the owned image:

```sh
hdiutil detach "$PWD/build/filesystem-case-sensitive"
```

## Windows x64 and preserved Win32

Use the corresponding x64 or x86 MSVC developer command environment and a short
build path (the existing Windows backend uses legacy MAX_PATH limits):

```powershell
cmake -S tests/native/filesystem -B build/filesystem-x64-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/filesystem-x64-release --parallel
ctest --test-dir build/filesystem-x64-release --output-on-failure
```

Repeat in Debug and in the Win32 compiler environment. The POSIX source items
remain excluded by the Windows CMake source selection. Windows ordering is compared
against independent native `FindFirstFileA` results on the generated directories;
the fixture does not assume POSIX order. Windows execution remains a separate
qualification step.

To compare a pinned reference, set `CSE_FILESYSTEM_POLICY_ROOT` to a preserved
repository with its owning CMake definitions and source tree, build target `filesystem_stdio`, then
run the current driver against that actual reference DLL:

```powershell
build/filesystem-x86-release/filesystem_tests.exe C:/owned/reference/filesystem_stdio.dll C:/owned/fixture pack
```

The executable accepts `module search files pack find case long handles
unload-find mount-lifecycle lifecycle` as the final argument. The supplied fixture
root is deleted and recreated by each invocation. Do not use a real game or user
data directory.

## Preserved behavior and limits

POSIX enumeration preserves reverse-alphabetic order within each path and keeps
duplicates across paths: `same.txt, a.txt, same.txt, b.txt` in the generated case.
No deduplication or handle registry is introduced. File handles follow the
existing valid ownership contract: one successful Open, one Close, no subsequent
use. Null Close retains its existing warning/tolerance. Arbitrary and double
Close remain inherited invalid-ownership defects and are not qualified as safe.

The Steam-prefix unit uses a controlled home-path argument and owned symlink; it
does not modify HOME, CODEX_HOME, or real Steam data. It checks absent-link
ordinary descent and the valid nonempty canonical-prefix predicate. The actual
module also checks exact missing-file failure under its existing environment.

The shared symbol table is scoped to live filesystem owners. Its final release
follows native-find, search-path and container cleanup; independently owned symbol
tables survive peer filesystem release. This retains the backend's existing
single-threaded ownership assumptions. These checks qualify a source module and
its stated API cases, not whole-game compatibility, all filesystem operations,
Unicode case folding, large-file behavior, or gameplay.
