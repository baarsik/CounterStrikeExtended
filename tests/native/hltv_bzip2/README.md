Builds the retained private HLTV bzip2 1.0.2 sources. This does not substitute
the separately packaged newer bzip2 dependency or change compression settings.
The fixture emits 48 deterministic compressed records, covering empty, byte
cycle, zero and seeded payloads across both 100 kB and 900 kB block sizes. Each
record decodes through both decompressor modes. An owned build-directory file
also exercises chunked stdio writes, close counters and reads.

```sh
cmake -S tests/native/hltv_bzip2 -B build/hltv-bzip2 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/hltv-bzip2
ctest --test-dir build/hltv-bzip2 --output-on-failure
```

Compare `vectors.bin` and `owned-stream.bz2` byte-for-byte across builds. Use
`-DCSE_HLTV_BZIP2_SOURCE=/path/to/pinned/original` for a frozen source reference
and `-DCSE_HLTV_BZIP2_SANITIZERS=ON` for ASan/UBSan. No network, provider or game
process runs in this fixture; compressed packet and full-game acceptance are
separate gates.
