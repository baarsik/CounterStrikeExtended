# Effects numeric safety

This fixture compiles the actual `efx` registration table and extracted production allocation,
particle retirement, temporary-entity update, visibility and beam bodies. Unrelated effects are
link-only stubs. Actual engine structures and vector arithmetic are retained; drawing, random
values and client update callbacks are controlled at their external boundaries.

The cases cover NaN and both infinities on every position/velocity axis, invalid scalar parameters,
unchanged finite spark/sprite behavior, pool ownership and death callbacks, invalid callback output,
independent model attachments, animated beam attachment recovery and segment conversion overflow.
The real particle simulation also exercises invalid ramps and reuse through `R_EntityParticles`,
which deliberately inherits velocity/ramp fields from the free pool.

```sh
cmake -S tests/native/effects_safety -B build/effects-safety -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_EFFECTS_SANITIZERS=ON
cmake --build build/effects-safety --parallel
ctest --test-dir build/effects-safety --output-on-failure
```

This does not exercise client collision callbacks or a GPU. The Metal renderer fixtures separately
cover all supported primitive topologies and compare actual pixels with and without invalid neighbors.
