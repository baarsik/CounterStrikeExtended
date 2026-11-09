# Native CD command track values

The fixture compiles the actual native MP3 track-argument function extracted from
`engine/cd.cpp`. Fifteen fixed input cases cover empty/sign/trailing text, normal
tracks, both signed endpoints and overflow. On Windows it additionally compiles
and compares the retained `atoi(argument) + 1` expression extracted from the same
command handler. The native helper preserves its 32-bit result and wrap without
signed-overflow behavior. Other native track commands use the separately
qualified `CseLegacyAtoi` conversion.

```sh
cmake -S tests/native/cd_tracks -B build/cd-tracks -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cd-tracks
ctest --test-dir build/cd-tracks --output-on-failure
```

Use Debug and `-DCSE_ENABLE_SANITIZERS=ON` in another directory for Clang
ASan/UBSan. These checks cover numeric command arguments, not command dispatch,
physical CD devices, decoding, mixing or audible output.
