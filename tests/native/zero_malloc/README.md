Compiles the complete actual `engine/mem.c` against a controlled allocator. It
checks allocation-size/identity and every byte of the zeroed region and retained
tail. A bounded child receives a null allocation and must diagnose then abort,
including Release; `assert` disappearing under `NDEBUG` must not permit execution
to continue. No system-memory exhaustion, real provider or UI is involved.

This fixture qualifies the POSIX failure boundary only. The Windows UCRT assertion
path remains unchanged. Release, Debug and optional ASan/UBSan builds use the
existing NoEH/unwind policy. Set `CSE_ZERO_MALLOC_SANITIZERS=ON` for instrumentation.
