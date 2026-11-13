# Studio mesh tests

Configure this test directory independently; it does not launch either game:

```sh
cmake -S tests/native/studio_batch -B build/studio-batch-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/studio-batch-tests
ctest --test-dir build/studio-batch-tests --output-on-failure
```

The optional `CSE_STUDIO_GL_PROBE=ON` target needs a graphical desktop, an OpenGL 2.1
compatibility context, and an existing SDL3 shared library specified with
`CSE_STUDIO_SDL_LIBRARY=/absolute/path/to/libSDL3.0.dylib` (or the platform's SDL3
library). It creates a hidden 128 × 128 window and exits after a bounded fixture
suite and a small benchmark. The CTest timeout also bounds the process.

`studio_gl_probe` compares original immediate-mode strips/fans against the real
production topology builder and indexed submitter. Its eight cases require exact
RGBA readback equality and restore foreign client-array enables, pointers,
formats, strides and active client texture unit. They cover smooth/flat shading,
odd strip winding, fans, complete attribute seams, front culling, additive and
masked states, combined masked/additive flags, and inherited constant shell
color. Terminal color and texture coordinates must match immediate mode even
when the last command has fewer than three vertices. Foreign normal, index,
edge, secondary-color, fog-coordinate and two texture-coordinate arrays are
all enabled before submission.

Optional command-line arguments name version-10 GoldSrc MDL files:

```sh
build/studio-batch-tests/studio_gl_probe /absolute/path/to/v_scout.mdl
```

For each file the benchmark reads the first model in each body part and every
mesh's original command stream. Both paths compute the same deterministic
synthetic attributes. Indexed timing includes warm-cache command validation and
attribute construction, in addition to all state checks and submission; both
paths finish the GPU at each timed batch boundary. Timing alternates which path
runs first. Use a Release build for performance comparisons.

The files supply topology only: this does not run game animation, real textures,
Lambert lighting, chrome calculation, glow-shell expansion, or gameplay. Timings
are per-model microbenchmarks and cannot establish gameplay FPS or acceptance
on another driver/platform. The synthetic workload has deliberately repeated
strips and unusually high tuple reuse. Source models and absolute asset paths
are not bundled with the tests.

The CPU engine-attribute fixture extracts the intact production helper, uses real
studio structures and checks ordinary/mirrored/chrome/forced-chrome attributes,
current alpha, updated CPU positions and changing UV scales. Its controlled Lambert
dependency checks which real input arrays the helper selects; it does not replace
production attribute assembly. Separate checks cover retained geometry truncation
and misaligned command offsets. `CSE_STUDIO_SANITIZERS=ON` enables ASan/UBSan for
the CPU fixtures. A compile case also preserves compatibility with Windows SDK
`min`/`max` macros.
