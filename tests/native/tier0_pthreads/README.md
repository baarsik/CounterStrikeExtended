# Actual tier0 pthread ownership

The complete production `tier0/threadtools.cpp` translation unit is compiled with
the actual generated Mac tier0 command from the runtime policy spike. The replay
changes only source/include/output paths and adds dependency-file generation;
the sanitizer profile also adds ASan/UBSan flags. Original and replayed command
arrays, database hashes and source hashes are retained beside the object file.
The spike's provisional policies are inputs to this component check, rather than
qualification of the complete runtime build policy or hardware-fault handling.

```sh
cmake -S tests/native/tier0_pthreads -B build/tier0-pthreads-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_TIER0_SPIKE_DATABASE=/absolute/path/to/build-debug/compile_commands.json
cmake --build build/tier0-pthreads-debug --parallel 2
ctest --test-dir build/tier0-pthreads-debug --output-on-failure -V
```

Use the matching Release database for Release. Set
`CSE_TIER0_PTHREAD_SANITIZERS=ON` for the separately linked, fully instrumented TU
and fixture. The fixture supplies only diagnostic `Msg` and fail-fast `Error`
bindings; all thread, TLS and synchronization implementations are production code
or actual system pthread operations.

The original private main-thread owner was an `unsigned int`, although the public
POSIX `ThreadGetCurrentId` and the `CThread` field already use `pthread_t`. On Apple
the latter is an opaque pointer. The bounded fix stores that existing native type
and compares live owners using `pthread_equal`. No public type, member, virtual
interface or Windows-selected code changes.

Four scenarios check native identities with actual high address bits, initial
ownership, synchronized transfer and restoration of main-thread ownership,
`CreateSimpleThread` result/join ownership, actual `CThread` initialization/TLS,
results and one `OnExit` per run, and eight reuse cycles. Gates avoid concurrent
updates of the inherited main-thread owner. These tests do not establish that
arbitrary concurrent `DeclareCurrentThreadIsMainThread` calls are safe.

For `CThread`, the fixture captures the owned native thread while `Run` is gated,
then joins that pthread exactly once. This avoids the inherited practice of
clearing `m_threadId` inside `ThreadProc` before exit. The completed/null-ID
`CThread::Join` path is checked; timed joins, cancellation, failure-to-initialize
cleanup, exception paths and the nonzero-ID `CThread::Join` path are not qualified
or changed by this task.

`verify_windows_selection.py` checks identical selected Windows source bytes and
tokens and unchanged project headers against the frozen V11 inputs. Existing
actual Win32/x64 Release/Debug build and binary receipts are referenced separately.
This lane makes no new Windows compiler invocation, VM action, module/provider
launch or game-runtime claim.
