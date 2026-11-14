# Bounded visibility fixtures

From a configured macOS ARM64 runtime build with `compile_commands.json`:

```sh
python3 tests/native/pvs_decompression/run_tests.py build/runtime-macos-arm64-release
python3 tests/native/pvs_decompression/run_tests.py build/runtime-macos-arm64-release \
  --assets '/absolute/path/to/Half-Life'
```

The runner compiles the actual `cmodel.cpp` visibility functions and the owning
hardware/software model-loader bodies with their configured runtime compilation
policies, assertions and ASan/UBSan. It extracts the actual inline-model setup loop
as well as the loader, slot reuse and teardown functions. Only allocation, error,
logging, byte-order, string and renderer/cache boundaries are controlled. It does
not initialize a game, graphics context, filesystem provider or engine module.

Cases cover literal and zero-run rows, strict truncations, zero-length runs,
null/empty rows, final padded runs, canaries, maximum logical rows, invalid
source/leaf bounds, native allocator ceilings, unaligned disk leaves, PVS/PAS
padding and unions, failed cache construction, unrelated cache owners, inline
metadata, reused model slots and map/session resets.

Optional assets are read-only. All local BSP leaf rows are compared with an
independent full-run expansion oracle, including null rows and physical counts
larger than the legacy visible-leaf limit. Stock maps with at most 256 visible
leaves additionally compare actual cached PVS/PAS rows with independent unions.
Other BSP parsing and graph traversal are outside this fixture's scope.

For baseline RED evidence, production bodies can be read from a Git revision:

```sh
python3 tests/native/pvs_decompression/run_tests.py build/runtime-macos-arm64-release \
  --source-revision 838742e --case empty
python3 tests/native/pvs_decompression/run_tests.py build/runtime-macos-arm64-release \
  --source-revision 838742e --case padded
python3 tests/native/pvs_decompression/run_tests.py build/runtime-macos-arm64-release \
  --source-revision 838742e --case cache-boundary
```

These respectively expose the empty-row write, padded-run overwrite and cache
statistics read past a 32-leaf logical row. Baseline invocations are expected to
fail; run them separately because sanitizers stop on the first error.
