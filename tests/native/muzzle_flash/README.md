# Muzzle-flash effect boundary

The fixture extracts the production `R_MuzzleFlash` body from `cl_tent.cpp` and uses
real engine entity structures. Allocation, model frame queries, randomness and
submission are controlled so invalid attachments cannot consume resources or reach
a renderer. It covers positive/negative infinity and NaN on every coordinate axis,
then verifies normal model selection, position, scale, material, color, animation,
lifetime and pool exhaustion.

```sh
cmake -S tests/native/muzzle_flash -B build/muzzle-flash-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/muzzle-flash-tests --parallel
ctest --test-dir build/muzzle-flash-tests --output-on-failure
```

Use a separate build directory with `-DCSE_MUZZLE_SANITIZERS=ON` for ASan/UBSan.
The fixture does not run studio animation or a GPU. The reported crash was also
reproduced from a local recording: a custom model contained a nonfinite attachment
coordinate, and its transformed muzzle flash caused Metal's vertex checks to reject
the frame. The corrected Metal and OpenGL runtimes both complete that recording.
