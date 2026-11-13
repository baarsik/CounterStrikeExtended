# Fast mutex owner-width fixtures

The production repairs change `CThreadFastMutex::TryLock`, final `Unlock` and the required
standard includes. Windows thread IDs and `m_ownerID` stay 32-bit. Acquisition
uses the Windows `LONG` compare-exchange operand, with a byte copy preserving
IDs above `INT_MAX` through its signed parameter. No field, exported tier0
declaration or recursion/depth rule changes. Final unlock uses a 32-bit
`InterlockedExchange` to publish protected writes before releasing ownership,
including under Windows ARM64's default ISO volatile semantics.

The November 13 release-ordering follow-up was reviewed statically only. No
fixture, compiler probe or build was executed for that change; the historical
fixture descriptions and results below do not qualify the new source.

On Windows the fixture compiles the **complete production threadtools.h** with
the real Windows SDK. Small platform/debug declaration shims remove DLL
import/export decoration and unrelated tier0 startup dependencies. They do not
replace Windows scalar types, the mutex class, its method bodies or its SDK
interlocked operation. Owned bindings implement `ThreadSleep` using `Sleep` and
`ThreadGetCurrentId` using `GetCurrentThreadId`; a thread-local override supplies
the explicit high-bit ID cases. An SDK pointer-CAS forwarding binding supports
running the archived old header as a Win32 reference; native old-header builds
still fail their width diagnostic under `/WX`. No production atomic implementation
is copied or changed. This fixture qualifies the header/storage/operation boundary, not
the tier0 DLL export/import boundary or the entire thread library.

The Windows suite checks:

- Actual class size 8, alignment 4, owner offset 0 and depth offset 4.
- Recursive volatile and const-volatile lock/unlock paths.
- A non-normal depth sentinel proving CAS touches only the owner operand.
- `0x80000000`, `0xffffffff` and `0xfedcba98` owner IDs, including a failed
  acquisition by another ID.
- Failed `TryLock` from another real thread, final release and successor ownership.
- Four-byte-aligned owner addresses at 4 modulo 8, with surrounding guards.
- Four real threads entering 16,000 protected critical sections.

Use the matching MSVC tools prompt for each x86, x64 or conditional ARM64 build:

```powershell
cmake -S tests/native/thread_mutex -B C:/CI/CSE-ThreadMutex/x64-Release -G Ninja -DCSE_TARGET_ARCH=x64 -DCMAKE_BUILD_TYPE=Release
cmake --build C:/CI/CSE-ThreadMutex/x64-Release --parallel
ctest --test-dir C:/CI/CSE-ThreadMutex/x64-Release --output-on-failure -V
```

Repeat Debug in a separate directory. Replace both `x64` values with `x86` or
`arm64` under the matching tools environment. Both configurations retain
`/W4 /WX`; there are no diagnostic suppressions. The archived original header
can be selected with `CSE_THREADTOOLS_HEADER` to reproduce its native compiler
failure without modifying the checkout.

On non-Windows hosts, `CThreadFastMutex` is otherwise a pthread-mutex typedef,
so compiling that path would not exercise the repaired class. The optional
portable fixture instead compiles the **unchanged extracted constructor/method
bodies** against explicit 32-bit owner/depth storage and 32-bit ID/atomic
bindings. It does not redefine C++ keywords or claim an LLP64 compiler/Windows
ABI. For the archived old method, IDs remain within DWORD range even though a
host-local `unsigned long` temporary can be wider. Its pointer CAS is modeled
with synchronized byte copies, safely exposing the owner-plus-depth operand
without executing a misaligned pointer atomic. The repaired body uses a real
compiler 32-bit compare-exchange primitive. Portable tests cover seven controlled
storage/body scenarios; the free-running contention test is Windows-only.

```sh
cmake -S tests/native/thread_mutex -B build/thread-mutex-storage-debug -G Ninja -DCSE_TARGET_ARCH=arm64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/thread-mutex-storage-debug --parallel
ctest --test-dir build/thread-mutex-storage-debug --output-on-failure -V
```

Use `CSE_THREAD_MUTEX_SANITIZERS=ON` in a separate portable build for ASan/UBSan.
Selecting the archived old header and running `ctest -R operand-isolation`
must fail because its pointer-width CAS also compares the depth sentinel.
This negative model result is separate from the recorded actual Windows x64
compiler failure. The portable binding also provides a sequentially consistent
32-bit exchange for the new final unlock. It does not qualify actual Windows
ARM64 instruction lowering or performance.
