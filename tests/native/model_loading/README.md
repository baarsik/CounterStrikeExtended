# Studio and sprite loader regressions

From the repository root, after configuring the runtime macOS ARM64 Release preset:

```sh
python3 tests/native/model_loading/run_tests.py build/runtime-macos-arm64-release
```

The runner compiles the actual `l_studio.cpp` and extracts the actual sprite loader functions from `gl_model.cpp` and `model.cpp`. It reuses their owning compilation policies and adds ASan/UBSan, assertions and fatal sanitizer errors. Controlled allocators return exactly the requested storage; the upload boundary reads and captures the supplied pixels and palette. No renderer, filesystem provider, engine dylib or game is initialized.

Fixtures exercise supported studio versions, the complete-header version fallback, textured and untextured models, gamma-expanded software palettes at odd byte offsets, single/group sprites with genuinely unaligned disk records, every strict truncated prefix of complete textured/group inputs, malformed counts/offsets/dimensions/palettes/types/intervals and native allocator overflow. Malformed inputs must reach no loader allocation or upload. The extracted real heap-reader body additionally exercises short-read cleanup, oversized heap-file rejection and the existing load interception callback in both native and legacy preprocessing branches. The latter runs on the host ABI; it is not a Windows x86 build. Both existing direct studio-loader consumer fixtures receive syntax checks.

An optional read-only asset corpus can run through the same controlled boundaries:

```sh
python3 tests/native/model_loading/run_tests.py build/runtime-macos-arm64-release \
  --assets '/Users/mmalakhouski/Library/Application Support/Steam/steamapps/common/Half-Life'
```

Only IDST and IDSP records are loaded; sequence-group MDLs have a different identifier and are skipped. Assets are never copied into the repository.

The baseline failures can be reproduced without editing the checkout:

```sh
python3 tests/native/model_loading/run_tests.py build/runtime-macos-arm64-release \
  --baseline --source-revision 6ff0d84 --mode hardware --case studio-short
python3 tests/native/model_loading/run_tests.py build/runtime-macos-arm64-release \
  --baseline --source-revision 6ff0d84 --mode software --case sprite-valid
python3 tests/native/model_loading/run_tests.py build/runtime-macos-arm64-release \
  --baseline --source-revision 6ff0d84 --heap-only
```

The first two cases can each be selected in either mode. They fail respectively with a studio-header heap overread and an unaligned sprite-descriptor read. The heap-reader case fails because the legacy path returns a partially initialized buffer. Baseline mode deliberately uses the original two-argument loaders. Production bodies are read from the selected revision; current unchanged disk/native structure definitions and the current test harness are used.

These tests bound loader input and allocation behavior up to the existing texture-upload boundary. They do not validate downstream GL upload algorithms, runtime studio bone/animation graphs, gameplay or provider initialization.
