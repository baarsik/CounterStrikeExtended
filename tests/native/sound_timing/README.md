Extracts the actual `GetSoundtime` and `S_Update_` methods from `snd_dma.cpp`,
using controlled DMA and painting callbacks. No audio device, SDL provider,
filesystem or network is used. Cases cover stereo/mono sample counting,
mix-ahead clamping, DMA wrap, the large-time reset, stopped/blocked playback and
native calls while DMA storage is absent. Win32's selected timing source remains
the reference; its undefined absent-storage path is not executed.

```sh
cmake -S tests/native/sound_timing -B build/sound-timing -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/sound-timing
ctest --test-dir build/sound-timing --output-on-failure
```

Use `-DCSE_SOUND_TIMING_SANITIZERS=ON` for ASan/UBSan. These are timing and
lifecycle component checks, not mixer PCM, device or full-runtime acceptance.
