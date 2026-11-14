# WAV loading and resampling

This component compiles the complete production `snd_mem.cpp` against actual
SDK declarations. It also compiles the production `COM_LoadFileLimit` and
`Wavstream_Close` bodies with controlled filesystem, cache and temporary-memory
services. It opens no audio device, game or external provider.

Cases cover complete 8/16-bit PCM, every truncated prefix of an authored WAV,
invalid format fields and chunk lengths, RIFF containment, cue/loop bounds,
partial streamed data, padded and legacy unpadded Cool Edit loops, exact PCM
conversion, fixed-point overflow, complete output clearing, and file/read-buffer
ownership through successful loads, rejected files, allocation failure and
stream refill failure. Every bundled bot WAV is also parsed as a compatibility
corpus. Assertions use authored metadata and PCM bytes rather than the parser's
own calculations.

```sh
cmake -S tests/native/wav_loading -B build/wav-loading-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_WAV_SANITIZERS=ON
cmake --build build/wav-loading-sanitized --parallel
ctest --test-dir build/wav-loading-sanitized --output-on-failure
```

Use a separate Release build with sanitizers disabled. On Windows, use the
matching MSVC developer environment. `CSE_WAV_SOURCE` may select a preserved
`snd_mem.cpp` for negative controls. These component checks do not qualify
physical audio playback or the full engine resource-download path.
