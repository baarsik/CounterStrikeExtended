# A3D mixer floating-state boundary

This standalone fixture tests the actual `a2d.cpp` mixer. On Windows, the subject TU includes the entire production file and the exact `Mixer_MultiplyFixed16` body extracted from `A3dGeom.cpp`. The owning A3D CMake target is queried for the source's `/O2 /fp:precise /Oi` policy and Win32 `/arch:IA32`; the selected policy and source hashes are emitted into the build directory. The mixer remains optimized in Debug, matching its retained per-file settings. The independent state caller uses `/fp:strict`, explicit Release `/O2` or Debug `/Od /Zi`, and `/W4 /WX`.

Configure from an initialized MSVC environment, selecting the actual compiler CPU:

```powershell
cmake -S tests/native/a3d_float_state -B build/a3d-float-x64-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64
cmake --build build/a3d-float-x64-release
ctest --test-dir build/a3d-float-x64-release --output-on-failure -V
```

Use `x86` or `arm64` with the matching compiler environment, and repeat with `Debug`. `CSE_A3D_MIXER_SOURCE` selects a preserved actual mixer file for negative/reference probes. The header/table inputs are taken from that file's directory. No SDL, output device, playback, COM provider, game process, or engine presentation binding is involved.

On macOS, the component adapter replaces only the unused `WallEdge.h` geometry-class include with its actual integer-helper declaration, supplies basic Windows LLP64 scalar declarations, and pins the source's Windows `LONG_MIN/MAX` limits. All mixer function bodies and tables are retained. The host `long` and `lrint` remain native LP64; the selected finite PCM inputs fit both domains. This component result does not qualify the entire A3D module on macOS or conversion behavior outside those selected inputs.

```sh
cmake -S tests/native/a3d_float_state -B build/a3d-float-mac-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/a3d-float-mac-release
ctest --test-dir build/a3d-float-mac-release --output-on-failure -V
```

Repeat with `Debug`; add `-DCSE_A3D_FLOAT_SANITIZERS=ON` for Clang ASan/UBSan. The state test reads/writes real MXCSR or native FPCR/FPSR and checks sixteen combinations of rounding, denormal/NaN control, exception status, and masked/unmasked invalid-exception control. It restores the original environment before reporting results. Win32 additionally seeds x87 status/precision/rounding and verifies the original reset behavior independently of SSE. The ARM64 method preserves FPCR/FPSR because there is no separate x87 environment to reset.

The pinned MSVC 14.51.36231 / Windows SDK 10.0.26100.0 x64 CRT differs from the failure behavior described in the Microsoft `_controlfp_s` documentation. Actual `_controlfp_s(_PC_64, _MCW_PC)` returns zero with `errno=0` and no invalid-parameter callback in both configurations; Debug emits the assertion `(mask&~(_MCW_DN|_MCW_EM|_MCW_RC))==0`. The separate x64 precision probe records that exact observed behavior. The actual original mixer fails the Debug state test on that assertion; Release preserves MXCSR. The candidate omits the unsupported precision request on x64, retaining the complete MXCSR save/reset/restore. The Win32 statements remain unchanged. The Debug report hook handles reports only inside this fixture process so the negative probe needs no UI; qualified state and PCM tests require zero reports and callbacks.

The 32 checked-in PCM files are owned synthetic original Win32 receipts: 8/16-bit mono/stereo, direct/convolved+filtered, loop/tail, 22/44 kHz selection. Each contains 112 stereo frames (448 bytes). All original Win32 Release/Debug and x64 Release/Debug captures were byte-identical. Candidate comparisons require exact bytes, with no numerical tolerance. `fixtures/manifest.json` records the original source hash, policy and individual hashes. `write-pcm DIRECTORY` is an explicit reference-capture mode; ordinary CTest only compares existing references.

The source fixture allocates four deterministic guard frames before and after its 64 logical input frames. Existing boundary/cubic readers may inspect these samples. An initial unpadded capture exposed a last-frame read beyond the allocated vector and was discarded as parity evidence; production readers were not changed by this task. ASan/UBSan qualifies the padded owned inputs, not general unpadded-reader safety. This coverage does not establish mixer parity for all inputs, real-time output, spatial-provider support, native A3D factory availability, startup audio, or full-game behavior.

Primary platform references: [Microsoft `_controlfp_s`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/controlfp-s?view=msvc-170), [Microsoft floating-point compiler policy](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170), and [native Windows ARM64 ABI](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170). The actual pinned CRT receipts remain authoritative for the observed diagnostic behavior above.
