# HPAK v1 archive compatibility

This fixture executes the complete engine `hashpak.cpp` and the actual engine MD5
implementation in `crc.cpp`. Typed in-memory filesystem, allocator, console and
command bindings isolate the file-format boundary. They do not qualify the VFS,
network downloads, WAD parsing, spray rendering or the complete client.

`make_fixture.py` independently authors a 12-byte header, one 1024-byte payload,
a 4-byte count and a 144-byte Win32 entry. The 136-byte resource contains fixed
fields, two padding bytes and two ignored 32-bit linked-list pointer slots. No
native structure layout supplies the oracle. Updated writers zero padding and
pointer placeholders. Nonzero legacy pointer bits are discarded on import.

The current fixture covers exact fields/payloads, create, insertion at the start,
middle and end, first/middle/last removal, duplicate add, queued duplicates and
multiple archives, source copying, list, validate, extract and item lookup. It
also checks all seven resource types, every truncation of the authored file,
malformed signed offsets/counts/lengths, short reads/writes, duplicate directory
hashes, compaction overflow and pointer-bearing native inputs. Sparse logical
positions exercise a directory tail above 2 GiB without a large allocation or
claiming a real VFS large-file test. Native heap allocations must exceed 4 GiB.
It saves `created.hpk`, `two-entries.hpk` and `four-entries.hpk` for comparisons
with preserved Win32 writers.

Two existing console-owner behaviors are characterized in both versions:
`HPAK_RemoveLump` selects `Cmd_Argv(1)`, and its one-entry path deletes the archive
before matching the requested hash. They are not layout fixes. The multi-entry
missing-hash path now frees its temporary native directories. Short I/O prevents
publishing a replacement; rename/flush failure behavior remains a separate VFS
gate. The global legacy `COM_CopyFileChunk` is unchanged; HPAK uses a local
checked copy. The reference fixture binding retains the legacy helper's ignored
byte-count behavior, and unrelated CRC file functions abort if reached.

```sh
cmake -S tests/native/hpak_records -B /tmp/cse-hpak -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build /tmp/cse-hpak
ctest --test-dir /tmp/cse-hpak --output-on-failure
```

For a preserved complete Win32 owner, set `CSE_HPAK_SOURCE` to its immutable copy
and `CSE_HPAK_MODE=reference`. This runs only complete valid-file cases. The old
native-width reader's defined empty-payload failure is separately captured by
`read_fixture.cpp`; malformed legacy reads are never executed.
