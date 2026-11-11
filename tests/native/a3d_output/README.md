# Native A3D platform/output ownership fixtures

This fixture compiles the real `native_platform.cpp` and `native_output.cpp` owners.
`fixture_mixer.cpp` is a labeled deterministic test producer implementing only the
private `A3dNativeMixer::Render` declaration. It is never selected by the retained
production manifest. Task 4 must implement the actual mixer before module linking
can complete. No test in this directory qualifies a real audio device, OEM backend,
complete module or game consumer.

Configure with the **existing** SDL3 3.4.20 build's package directory:

```sh
cmake -S tests/native/a3d_output -B build/a3d-native-20261111/output-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DSDL3_DIR=/absolute/existing/build/dependencies/SDL3/upstream
cmake --build build/a3d-native-20261111/output-release --parallel 1
ctest --test-dir build/a3d-native-20261111/output-release --parallel 1 --output-on-failure
```

Repeat with `Debug` / `output-debug`. For the scoped ASan/UBSan lifetime run,
configure an additional `output-sanitize` directory with `-DA3D_SANITIZERS=ON`.
All builds and nested helpers are serial. No dependency installation is required.

CTest explicitly sets `SDL_AUDIO_DRIVER=dummy` for output cases. Most output tests
inject operations that use a real, unbound SDL conversion stream; failure injection
is confined to init/open/resume/put/thread boundaries. Tests exercise the production
owner's return values and teardown effects. The dummy-stream and enumeration cases
use the real SDL open/resume/destroy operations with the dummy driver. They cannot
establish physical playback. The callback never invokes the fixture mixer.

The output contract uses app-side 22050-Hz stereo S16 PCM, eight 256-frame prepared
blocks, one producer and an open-paused SDL transport. Start propagates SDL resume
failure and closes the partial owner. Close stops/joins production, fences/removes
the SDL callback, destroys its stream and queue state, then releases only its own
SDL reference. Open/Start/Close/DeviceGuid require external serialization; the mixer
outlives Close. Injection functions must honor SDL's contracts and return actual
SDL stream objects. DeviceGuid retains this owner's audio reference so enumeration
IDs remain live; GUIDs are copied process-local tokens, never persistent identities.
Close retires that reference. Unknown and stale non-null tokens fail; null chooses
the actual SDL default.

Wave tests cover short headers/data, RIFF bounds, missing files, allocation failure,
valid PCM, odd chunk padding, format validation and descriptor cleanup. The exact
native `CA3dSource::LoadWaveFile` body is extracted unchanged into a labeled local
buffer-double fixture; it verifies split-lock copy and all-view release after
allocation, lock or unlock failure. This checks the caller body, not COM ABI/provider
acceptance. The native parser supports the retained mono/stereo PCM 8/16-bit domain.

The manual real-device probe is defined but is deliberately **not** a CTest:

```sh
# Controller/Task 8 only, with explicit real-device authorization and SDL driver unset:
cmake --build build/a3d-native-20261111/output-release --target a3d_manual_device_probe --parallel 1
```

It uses the same test-only deterministic producer for a bounded transport probe;
actual-module and game acceptance remain separate. This target was not executed
while implementing Task 2.
