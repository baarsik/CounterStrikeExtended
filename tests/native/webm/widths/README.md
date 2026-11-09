# WebM storage widths and allocation failure boundary

This component fixture exercises the actual PCM allocation helper and extracted
SDK demux methods. It performs no SDL initialization, presentation, sound,
network, Steam, or device calls. The separate parent WebM codec fixture still
compiles the entire TU with exceptions enabled; its decode results do not prove
the production TU's exception policy.

`webm_video.cpp` retains its no-exceptions policy. Native C++ objects now carry
unwind tables, including the startup-video caller chain; the narrow
`webm_pcm_allocation.cpp` helper has exceptions enabled. The controlled unwind
test verifies that `bad_alloc` and `bad_array_new_length` reach an outer catch
through a no-EH frame without running that frame's destructors. It does not run
`CEngineAPI::Run` or claim complete engine cleanup after allocation failure.

## Measured bounds

The pinned libwebm is 1.0.0.32. Its Track parser admits unique numbers 1..127;
Track count/index and Cluster status retain the library's `unsigned long`/`long`
types. `Block::Frame::len` is a positive `long`. Windows rejects frames larger
than `INT_MAX` in the parser, while LP64 libwebm admits them. The wrapper rejects
this expanded native domain before narrowing, allocation, or payload read.

The real MSVC Win32 `new short[count]` expression reaches its allocator with
`count * 2` through `count == 0x7fffffff`. Counts `0x80000000` and `UINT_MAX` use
the compiler's `SIZE_MAX` sentinel; the default runtime throws
`bad_array_new_length`. The default runtime throws `bad_alloc` at the admissible
`0x7fffffff` request on the measured Win32 process. The helper therefore derives
its count bound as `UINT32_MAX / sizeof(short)`, asserts both widths, rejects an
overflowing count with `bad_array_new_length`, and multiplies in `size_t` after
validation. Actual memory availability remains an allocation failure concern.
The former native `0x3f800000` bound was not the observed Win32 compiler bound.
Windows Win32 retains its original `new short[sampleCount]` expression; Windows
x64/ARM64 and POSIX use the helper.

These exception meanings also match the documented
[MSVC invalid array length type](https://learn.microsoft.com/en-us/cpp/standard-library/bad-array-new-length)
and [array allocation function](https://learn.microsoft.com/en-us/cpp/standard-library/new-operators).
The numeric limits above come from the recorded compiler/runtime probes.

The existing low32 Block track-ID comparison is deliberately retained. Actual
libwebm accepts `0x100000001` on all tested CPUs, and the original wrapper treats
it as track 1. This malformed-data behavior is recorded, not silently repaired
as a platform conversion.

## Tests and reference controls

`extract_demux_boundaries.py` records the input SHA-256 and extracts the actual
WebMFrame constructor/destructor, WebMDemuxer constructor/destructor/ReadFrame,
and the frame-size check. Its only production-call substitution is
`std::realloc` to `FixtureReallocate`, which injects one failed growth. A small
fixture forwarding function exposes the actual private size check. The SDK
bodies compile in a separate no-EH TU; the vector-based test driver has EH
enabled, so MSVC's standard-library templates do not dictate the SDK policy.

The growth test reads packets of sizes 1, 64, 64 through actual libwebm and the
actual SDK ReadFrame body. After the second allocation fails, the original
buffer pointer and capacity must remain valid. The third packet must retry
allocation. The old SDK body fails this check before any out-of-bounds write.

Sparse-container probes admit `INT_MAX` and `INT_MAX + 1` without allocating or
reading their payloads. The old native body attempts the expanded-domain read
and sets a negative frame size; a bounded fixture reader rejects it safely.
The current body rejects before attempting that read. Tests also cover negative,
zero, minimum, maximum, and LP64-only scalar lengths, and preserve the existing
Block alias. Large PCM count tests observe allocator requests by throwing a
controlled marker; no large allocation occurs.

The archived old-source extractor `prepare_allocation_reference.py` expects the
pre-change PCM block and guessed constant. It intentionally rejects a changed
source rather than silently extracting a different implementation. Use the
preserved `webm_video_before.cpp` for those diagnostic reference controls.

## Build

From the repository root, with a matching pinned dependency prefix:

```sh
cmake -S tests/native/webm/widths -B build/webm-widths-macos-arm64-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_NATIVE_THIRD_PARTY_PREFIX="$PWD/build/native-third-party/macos14/cse-arm64-macos"
cmake --build build/webm-widths-macos-arm64-release --parallel 2
ctest --test-dir build/webm-widths-macos-arm64-release --output-on-failure
```

Use a fresh Debug directory for Debug. ASan/UBSan uses a fresh Debug directory
with `-DCSE_WEBM_WIDTH_SANITIZERS=ON`. The helper and fixture headers are tested
as C++14. Actual generated production command receipts separately verify the
complete WebM TU and helper under both owning engine targets/configurations.
The macOS tests ran on the recorded host OS with deployment target 14; this is
not a macOS 14 runtime or video-presentation acceptance claim.
