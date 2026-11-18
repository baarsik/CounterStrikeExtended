# Native WebM demux and codec fixture

The executable includes the actual entire production `engine/webm_video.cpp` as
one translation unit. No decoder or demux method body is copied or extracted.
macOS discards unused player functions at link time. The Windows linker retains
their external-symbol obligations: the fixture links the real pinned SDL3 import
library/runtime. Signature-checked fixture bindings for engine logging, timing
and sound abort on every call. They
compile in a separate fixture translation unit against the actual engine headers;
the compiler sees the complete production WebM body without inferring that its
presentation calls always abort. A
subprocess test proves all seven bindings abort, including actual `PlayVideo`
entry at its first `Con_Printf`, before media/device/presentation activity. Crash
dialogs are disabled only in these fixture processes. No SDL presentation,
OpenGL, engine sound, or audio-device function is exercised. `SW_RENDERER` selects
the software branch during compilation.

The fixture uses the pinned source-built libwebm, libvpx, libvorbis and libogg
targets selected by `cmake/NativeThirdParty.cmake`. `selected-<config>.txt` records
the four exact codec link inputs, plus both SDL3 inputs on Windows. The native prefix's additional `include/webm` directory
is required by the installed mkvreader header's nested include. No host package
discovery fallback or dependency upgrade is introduced.

`fixtures/manifest.json` pins the SHA-256 of two wholly owned WebM files, independent
YUV/PCM references, and a truncated EBML file. `generate_fixture.cpp` generates the
encoded media with real pinned VP8, lossless VP9, Vorbis encoders and the libwebm
muxer. Input is an original integer YUV pattern and asymmetric rational stereo
samples. Independent direct decoder calls produce packed I420 planes and S16LE
PCM references. There are no downloaded clips, system assets, or recorded audio.
The qualified generator reproduced all five media/reference hashes exactly on a
second local run; regeneration is an explicit optional target.

Each WebM contains eight 16x16 video packets at 25 fps and 23 Vorbis packets,
yielding 8,192 decoded stereo frames at 48 kHz. Tests exercise real production
metadata, timestamps, keyframe flags, native codec-private data, selective
audio/video demux, EOF, decoding, delayed-frame flushing and decoded plane
geometry. YUV bytes must exactly match their independent references. PCM lengths,
channel order, scratch-buffer canaries and sample values are checked with a
one-LSB cross-platform tolerance; local qualified results had zero sample delta.
This tolerance does not claim byte-identical floating-point PCM on every CPU.

The checked native codec-private size conversion is a static production helper.
Tests exercise UINT_MAX, UINT_MAX+1 and SIZE_MAX without forging library objects
or allocating oversized data. Real parsed track data exercises the complete
GetCodecPrivate(size_t&) to GetAudioExtradata(unsigned int&) route. The public
unsigned-size signature remains unchanged. The regular original source produces
native layout/API compile failures; removing just the size guard produces the
retained oversize-boundary RED.

The fixture also links production `engine/yuv2rgb.cpp`. Synthetic 4:2:2 and 4:4:4
planes cover one-row frames, odd and even dimensions, distinct chroma on successive
rows, and untouched output padding.

Thread cases cover requests 0, 1, 8 and UINT_MAX through actual decode results.
Lifetime cases repeat the complete decoder/demux flow 64 times per codec, including
Vorbis Close/reopen and independently owned assignment to an empty WebMFrame.
Invalid-container cases repeat actual libwebm parse failure and empty packet
delivery. Optional Apple leak checks require fixture completion and zero leaked
bytes after the repeated codec lifetimes.

## Commands

From the repository root on Apple Silicon:

```sh
cmake -S tests/native/webm -B build/webm-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos" -DCSE_WEBM_LEAK_CHECKS=ON
cmake --build build/webm-release --parallel
ctest --test-dir build/webm-release --output-on-failure
```

Repeat with Debug and another build directory. `CSE_WEBM_RECEIPT_DIR` selects the
full leak-report directory. For ASan/UBSan, use a third Debug directory and add
`-DCSE_WEBM_SANITIZERS=ON` without the leaks option. The entire production wrapper
and fixture are instrumented; the existing pinned dependency archives retain their
original source-built Release/Debug instrumentation policy. No sanitizer
suppression is applied.

For Windows x86, x64 or ARM64, use the matching MSVC developer environment and
the corresponding pinned dependency prefix. Omit the CMAKE_OSX arguments and
Apple leaks option; select a distinct build directory. Also pass
`CSE_WEBM_SDL3_IMPLIB` and `CSE_WEBM_SDL3_RUNTIME` as absolute paths to the matching
CPU/configuration source-built SDL3 import library and runtime DLL. Both inputs
are checked for target architecture, and the DLL is copied beside the fixture.
There is no host-package or other-configuration fallback. The fixture calls only
SDL version/revision metadata, verifies its version matches the compile-time
header, and reports the actual loaded DLL path. Windows uses /W4 /WX,
matching Debug/Release DLL CRTs, and function-level dead-code removal. `WIN32`
and an empty fixture `DLL_EXPORT` select existing header guards without exporting
the player from the test executable or changing production declarations.

To explicitly reproduce owned media in a separate owned directory:

```sh
cmake --build build/webm-debug --target webm_fixture_generator --parallel
build/webm-debug/webm_fixture_generator build/webm-debug/regenerated-fixtures
```

The generator is excluded from the default build. Checked-in fixture hashes are
the test inputs; generation is not used to update expected values during tests.

## Limits

This qualifies the stated wrapper methods and 8-bit I420 VP8/VP9 plus stereo
Vorbis cases. It does not qualify player presentation, startup video, sound
output, Opus, 10-bit/other-chroma decode, arbitrary malformed codec data, all
multitrack policies, or generalized WebMFrame copy/assignment behavior. Native
wrapper layout changes do not change the media format. macOS14 load metadata
does not prove execution on macOS14. Windows matrix and full game acceptance
remain separate qualification steps.
