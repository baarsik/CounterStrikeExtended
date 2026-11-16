# Native radar behavior tests

The geometry, reachability, layer generation/selection/override, projection, policy,
polyline and utility cases and map fixtures are adapted from NextClient commit
`1d72f0a5f576ffd0e99207ac4edfec7468c6c218`. See the port's
provenance notice at `src/cstrike/cl_dll/nextclient/`.

`Test.h` provides the assertion/registration subset these cases need without a GTest
or runtime dependency. Both original fatal and nonfatal assertions stop the current
case at the first failure; the other cases still run. Float equality retains the
original four-ULP tolerance. Scoped traces are included in failure messages.

Native worker cases additionally cover owner-thread publication, canceled results,
coalescing superseded requests, reset/reuse and destruction without frame pumping.
The target compiles the actual radar production bodies rather than algorithm copies.
Layer-context cases also verify that standing/crouching feet use the collision state
when rendering traces have overwritten the temporary trace hull.

Run from the repository root:

```sh
cmake -S tests/native/radar -B /tmp/cse-radar-tests -G Ninja
cmake --build /tmp/cse-radar-tests
ctest --test-dir /tmp/cse-radar-tests --output-on-failure
```

Pass `-DCSE_RADAR_SANITIZERS=ON` on Clang/GCC to enable AddressSanitizer and
UndefinedBehaviorSanitizer.

To exercise the actual radar shaders, glyph clipping, marker state handoff, blur,
generated textured/flat map images and context replacement against the existing SDL3
runtime and a real desktop OpenGL driver:

```sh
cmake -S tests/native/radar -B /tmp/cse-radar-gl-tests -G Ninja \
  -DCSE_RADAR_GL_TESTS=ON \
  -DCSE_SDL3_LIBRARY="$PWD/build/runtime-macos-arm64-release/runtime/Release/libSDL3.0.dylib"
cmake --build /tmp/cse-radar-gl-tests
ctest --test-dir /tmp/cse-radar-gl-tests --output-on-failure
```

The optional target creates hidden SDL3 OpenGL 2.1 windows and offscreen framebuffer
images; it does not launch the game. It uses actual production renderer/GL-loader
bodies and generated shader strings. BSP fixtures verify that fallback maps ignore
brush submodel geometry, sky caps and height ranges. Label cases run the production
HUD text buffer and exclusion zone, checking fallback positions and passthrough restoration.
Engine services are limited to shader-error logging, a console-text capture sink and
an unavailable lightmap atlas for the flat BSP fixtures.
