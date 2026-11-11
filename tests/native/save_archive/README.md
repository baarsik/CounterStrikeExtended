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
empty archives. Malformed counts, lengths, terminators, traversal, field/payload
truncations, trailing data, ambiguous complete layouts, forged layouts, failed/incorrect
cursor progress, signed seek bounds and injected open/read/write/copy failures are
covered. Original input bytes remain intact. Validation failures open no output files;
later IO failures may leave partial outputs and return failure, including SaveGameSlot.

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
is accessed by these fixtures. Windows fixture execution and actual cross-platform
campaign/global/entity roundtrips remain separate controller gates.
