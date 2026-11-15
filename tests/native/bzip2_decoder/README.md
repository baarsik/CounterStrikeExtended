# Bundled bzip2 decoder

This fixture compiles the actual bundled bzip2 sources used by Core and the HLTV
proxy. It checks rejection of selector counts outside the decoder's array bounds,
acceptance of the minimum and maximum valid counts, and exact round trips through
both decompression modes with the minimum and maximum compression block sizes.

The count cases resume the real decoder at its selector-count field with no
selector payload. Valid counts request more input; invalid counts must return
`BZ_DATA_ERROR` immediately. No malformed compressed file is created and no
out-of-bounds write is exercised, including when checking an unfixed decoder.
The fixture does not qualify engine networking, demo playback or gameplay.

Run from the repository root:

```sh
cmake -S tests/native/bzip2_decoder -B build/bzip2-decoder-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/bzip2-decoder-release --parallel
ctest --test-dir build/bzip2-decoder-release --output-on-failure
```

Use a separate Debug directory with `-DCMAKE_BUILD_TYPE=Debug` and, with Clang or
GCC, `-DCSE_BZIP2_SANITIZERS=ON` for ASan/UBSan. On Apple Silicon add
`-DCSE_TARGET_ARCH=arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`.
On Windows use the matching MSVC target environment and `-DCSE_TARGET_ARCH=x86`
or `x64`.
