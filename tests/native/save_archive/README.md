# Save archive owner fixture

This CPU fixture compiles the real `save_archive_impl.h` and source-extracted complete
`Host_Load`, `SaveGameSlot`, `DirectoryCopy`, and `DirectoryExtract` bodies.
`fixture.h` substitutes only filesystem leaves and unrelated game/envelope services;
no substitute archive parser or writer is present. The Host_Load envelope/global/entity
reconstruction is a separate runtime gate. The fake filesystem models byte-returning
FS_Read/FS_Write and void FS_Seek with checked cursor observations.

Run each command serially, including compiler helpers:

```sh
cmake -S tests/native/save_archive -B build/save-archive-20261111/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/save-archive-20261111/release --parallel 1
ctest --test-dir build/save-archive-20261111/release --parallel 1 --output-on-failure
cmake -S tests/native/save_archive -B build/save-archive-20261111/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/save-archive-20261111/debug --parallel 1
ctest --test-dir build/save-archive-20261111/debug --parallel 1 --output-on-failure
cmake -S tests/native/save_archive -B build/save-archive-20261111/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCSE_ARCHIVE_SANITIZE=ON
cmake --build build/save-archive-20261111/sanitize --parallel 1
ctest --test-dir build/save-archive-20261111/sanitize --parallel 1 --output-on-failure
```

Both 260 and 1024 host-limit configurations compare canonical bytes with independently
placed 260 filename/four-byte little-endian fields. They also read both widths with
nonzero old padding, zero payloads, nonzero archive starts, exact EOF, long names and
empty archives. A unique 260 or 1024 layout may also contain complete byte-identical
copies of the declared first set. Every repeated byte, including name padding, length
fields and payload, must match. The writer still emits exactly one canonical 260 set.
Malformed counts, lengths, terminators, traversal, field/payload
truncations, partial/differing/arbitrary trailing data, ambiguous layouts (including
exact versus repeated interpretations), forged layouts, failed/incorrect
cursor progress, signed seek bounds and injected open/read/write/copy failures are
covered. Original input bytes remain intact. Validation failures open no output files;
later IO failures may leave partial outputs and return failure, including SaveGameSlot.

Private `Layout.end` is physical EOF; `recordsEnd` ends the declared first set.
ReadLayout checks both widths and restores the initial source cursor on success. It
clears the returned layout on failure and attempts cursor restoration after candidate
validation; a failed restoration cannot promise a cursor position. Extract revalidates
the complete physical layout before opening output, writes only the first set, and
leaves the source at physical EOF on success. A final EOF-seek failure can therefore
return false after all output payloads have been written. Opened outputs are closed on
copy failure, but partial outputs are not rolled back; the input remains caller-owned.
Canonical preflight checks headers and seeks over payloads, so extraction-time read
faults can also leave partial output. Layout metadata does not authenticate the input
against concurrent mutation or a same-sized replacement with another valid archive.

Repeated-set tests cover two, three and additional copies, nonzero archive starts,
padding, empty and multi-chunk payloads, long names, width ambiguity, changed metadata
and source extents, and targeted read/seek failures in both width configurations.
IO observations check bounded read requests and a linear total-byte bound. The separate
host_reload fixture retains failure-lifecycle cases with a genuinely differing repeated
payload and separately checks that identical repeated input loads without fallback.

An explicit read-only corpus mode accepts a path and independently established archive
start, declared count, first-set end and physical end:

```sh
build/save-archive-20261111/release/archive_1024 --corpus /absolute/path/quick.sav 4458 18 291981 867027
```

Record the input SHA256 before and after this opt-in invocation. The fixture reads the
file into memory, independently decodes first-set record payloads, then runs production
ReadLayout and Extract with all outputs confined to the in-memory filesystem. This is
actual-corpus archive-reader evidence; it does not validate the full save envelope,
global/entity restore, or gameplay. Ordinary CTest cases use generated input only.

To reproduce the original defect without changing any production source:

```sh
mkdir -p build/save-archive-20261111
git show baddde12b3924e6a04adaaf0c01946f557f96703:GoldSrc-SDK/engine/host_cmd.cpp > build/save-archive-20261111/old-host_cmd.cpp
cmake -S tests/native/save_archive -B build/save-archive-20261111/save-archive-red -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_ARCHIVE_OLD=ON
cmake --build build/save-archive-20261111/save-archive-red --parallel 1
ctest --test-dir build/save-archive-20261111/save-archive-red --parallel 1 --output-on-failure -R 'old-1024-(writer|reader)'
cmake -S tests/native/save_archive -B build/save-archive-20261111/save-archive-red-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCSE_ARCHIVE_OLD=ON -DCSE_ARCHIVE_SANITIZE=ON
cmake --build build/save-archive-20261111/save-archive-red-asan --parallel 1
ctest --test-dir build/save-archive-20261111/save-archive-red-asan --parallel 1 --output-on-failure -R old-1024-short
```

These RED tests intentionally fail. The original source SHA256 is pinned in `extract.py`;
all owner-body boundary anchors must be unique and ordered. The original writer test
uses padded find-name storage solely to isolate the wrong-width failure. The separate
ASan `short` test uses an actual ten-byte heap string and exposes the original 1024-byte
padding overread. GREEN writer tests use short heap strings throughout.

The source archive path may be overridden with `CSE_ARCHIVE_OLD_SOURCE`, but its digest
must still match. No original save, game asset, device, VM, provider or live application
is accessed by ordinary CTest cases; the explicit corpus mode only reads its supplied
file. Windows fixture execution and actual cross-platform
campaign/global/entity roundtrips remain separate controller gates.
