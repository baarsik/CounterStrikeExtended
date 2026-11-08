# Native dependency source pins

This manifest preserves the selected Win32 package versions and features. Baseline
`20ceb8adbf0504d4b0d5e4058a32f9d4fc896391` matches all 120 recorded recipe file
hashes across the retained target and host package SPDX records. It is a verified
equivalent snapshot; the original vcpkg checkout revision was not recorded.
The SPDX creator hash identifies **vcpkg-tool**, not the ports registry.

| Package | Pinned version | Explicit features |
| --- | --- | --- |
| brotli | 1.2.0 | core |
| bzip2 | 1.0.8#6 | core, tool |
| expat | 2.8.5 | core |
| freetype | 2.14.3 | core, brotli, bzip2, png, zlib |
| libpng | 1.6.59 | core |
| zlib | 1.3.2#2 | core |
| libogg | 1.3.6#1 | core |
| libvorbis | 1.3.7#4 | core |
| libvpx | 1.16.0#3 | core |
| libwebm | 1.0.0.32 | core |
| dirent (Windows) | 1.26 | core |
| fontconfig (POSIX) | 2.17.1#2 | core |

Defaults are disabled: this retains FreeType without HarfBuzz, libpng without
APNG, VPX without high-bit-depth/realtime-only options, and fontconfig without
iconv/NLS/tools. Source archive SHA512 and recipe/patch SHA256 values are in
`GoldSrc-SDK/external/installed/x86-windows-static-md/share/<package>/vcpkg.spdx.json`.
The pinned recipes verify downloaded source archives before extracting them.

Use an isolated vcpkg checkout; do not update its registry while qualifying:

```sh
git init build/native-vcpkg
git -C build/native-vcpkg remote add origin https://github.com/microsoft/vcpkg.git
git -C build/native-vcpkg fetch --depth 1 origin 20ceb8adbf0504d4b0d5e4058a32f9d4fc896391
git -C build/native-vcpkg checkout --detach FETCH_HEAD
build/native-vcpkg/bootstrap-vcpkg.sh -disableMetrics
build/native-vcpkg/vcpkg install --triplet cse-arm64-macos --overlay-triplets=cmake/dependencies/vcpkg/triplets --binarysource=clear \
  --x-manifest-root=cmake/dependencies/vcpkg \
  --x-install-root=build/native-third-party/macos14
```

On macOS, the pinned gperf recipe requires `autoconf`, `autoconf-archive`,
`automake` and `libtool` host tools. Windows uses `bootstrap-vcpkg.bat` and
`vcpkg.exe`, with `x64-windows-static-md` or `arm64-windows-static-md`. The Windows
triplets preserve static dependency libraries and DLL CRTs (`/MD`, `/MDd`). Both
Debug and Release are built. Do not set `VCPKG_BUILD_TYPE` to remove one.

Audit the resulting install prefix without executing a provider:

```sh
python3 tools/qualification/audit_native_dependencies.py \
  --prefix build/native-third-party/macos14/cse-arm64-macos \
  --architecture arm64 --platform macos --output /absolute/path/to/new-report.json
```

The macOS triplet pins deployment to 14.0. The unmodified upstream `arm64-osx`
triplet otherwise inherits the host SDK minimum and is not a product build
contract. Every Mach-O archive member is checked for its actual minimum version.

The audit compares recipe/source identities with the retained Win32 provenance
and inspects actual archives in both configurations. It does not prove
source-to-binary reproducibility or runtime equivalence by itself. Preserve the
build logs and vcpkg status/SPDX records with the report.

A separate native fixture links the selected libraries and exercises compression
round trips, XML parsing, PNG pixels, FreeType lifecycle, Ogg packing, Vorbis
initialization, VP8/VP9 decoder lifecycle and WebM version access:

```sh
cmake -G Ninja -S tests/native/third_party -B build/third-party-macos-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/third-party-macos-release
ctest --test-dir build/third-party-macos-release --output-on-failure
```

Use separate build directories for Debug and other targets. Import resolution
checks both actual archive architecture and that selected libraries stay within
the requested prefix. This fixture does not render fonts/video or qualify game
behavior. Running it is an explicit standalone fixture action, not a build step.
