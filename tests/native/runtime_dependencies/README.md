# Native runtime dependency routing

`cmake/NativeRuntimeDependencies.cmake` provides two native-only functions:

- `cse_link_native_runtime_import(target, configuration, reference_path)` accepts
  the 24 exact retained Release/Debug logical archive paths and links their
  source-built package targets with the existing configuration guard.
- `cse_native_runtime_include(reference_include, output_var)` maps the retained
  `external/installed/x86-windows-static-md/include[/subdir]` tree to the
  selected native prefix. Other SDK paths are returned unchanged. Call it on
  plain manifest paths before adding generator-expression configuration guards,
  including source-specific include directories.

Both require an explicit native compiler target and SDK root. The native
dependency prefix must be explicit; package loading, archive architecture and
Release/Debug checks reuse `NativeThirdParty.cmake`. No version, source-list,
runtime behavior or x86 routing policy changes are made by these functions.
Logical x86 archive files need not exist on a native builder. Native packages
and their requested header directories must exist under the pinned prefix.

`RuntimeTargets.cmake` uses these functions for native retained imports and
target/source dependency includes. Win32 retains its original paths. The
translator resolves logical dependency includes before adding configuration
guards, preserving per-file include precedence.

The fixture exercises those runtime helpers and routes all 24 logical paths into twelve separate executables that
call real FreeType, bzip2, Expat, PNG, zlib, Brotli common/decoder, Ogg,
Vorbis/Vorbisfile, VP8 and WebM symbols. It checks the selected configuration in
actual Ninja link commands and compiler header dependencies. Poisoned legacy
FreeType/zlib headers detect incorrect include routing. Child configurations
exercise unknown imports/providers, configuration mismatch/fallback, path
escape, missing native headers/prefix and wrong archive architecture.

macOS ARM64 example:

```sh
cmake -S tests/native/runtime_dependencies -B build/runtime-dependencies-macos-arm64-release -G Ninja -DCSE_TARGET_ARCH=arm64 -DCMAKE_BUILD_TYPE=Release -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/runtime-dependencies-macos-arm64-release --parallel
ctest --test-dir build/runtime-dependencies-macos-arm64-release --output-on-failure
```

Repeat with Debug and a separate build directory. On Windows, initialize the
matching x64 or ARM64 MSVC tools prompt, use Ninja, select the matching pinned
source-built prefix, and pass the corresponding `CSE_TARGET_ARCH`. These probes
use the dynamic MSVC CRT matching the retained static-md package policy.

The original helper's attempt to validate a retained Win32 archive as a native
input was captured before integration, together with its failing receipt.
Passing this suite qualifies dependency routing and selected package symbols,
not a complete native runtime or game compatibility.
