# Counter-Strike random generator values

This fixture compiles the actual `cstrike/common/Random.cpp`, with only its clock
replaced by a deterministic test clock. It checks 152,064 encoded integer and
binary32 values against the original Win32 implementation. Original MSVC x86,
x64 and ARM64 Release/Debug runs produced the same reference corpus.

The corpus covers explicit seeds around both signed endpoints, the 1,000 seed
normalization boundary, native inputs with high bits set, eight clock values
including the signed and unsigned 32-bit wrap points, and nine integer ranges.
Each sequence checks 512 generator words, 512 floating-point outputs and 512
outputs per integer range. The expected digest is pinned in `verify_output.py`;
sequence order and counts are checked before hashing little-endian value words.

Windows retains its existing long-width arithmetic. Native builds use explicit
32-bit state and seed words, including the original zero-seed clock fallback and
INT32_MIN normalization. Unsigned range arithmetic preserves Windows wrap values
without C++ signed-overflow behavior. No distribution, rejection rule, constants
or floating-point expression changes are introduced.

```sh
cmake -S tests/native/randomness -B build/randomness-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/randomness-release
ctest --test-dir build/randomness-release --output-on-failure
```

Repeat with a separate Debug directory. Clang ASan/UBSan uses
`-DCSE_ENABLE_SANITIZERS=ON`. `CSE_RANDOM_IMPLEMENTATION` can select an immutable
copy of the actual source for reference qualification. These are isolated RNG
value checks, not complete gameplay, random-provider or cross-platform numeric
qualification.
