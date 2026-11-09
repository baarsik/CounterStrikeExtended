# Tier0 diagnostic identities and profiler TLS

This standalone fixture compiles the actual `threadtools.h` and `vprof.h` class
declarations. It extracts complete production constructor, TLS, GetName/SetName
and profiler Start/Stop/EnterScope/ExitScope bodies without rewriting their C++.
The platform's actual Windows TLS APIs or pthread keys back the exercised TLS.
DLL decorations are neutralized so those bodies bind into the executable; native
platform types, headers and inline mutex methods remain actual production code.

The diagnostic formatter checks the number of conversions before forwarding to
the real `_snprintf` on Windows or `snprintf` on POSIX. This safely exposes the
old `Thread(%0x%x/0x%x)` defect: **three conversions, two arguments**. It never
executes undefined missing-varargs reads. Native names retain both full identities
with `%p`; Win32 uses intended `Thread(0x%x/0x%x)` text. Event assertions use an
owned diagnostic sink which captures the typed argument even for a non-null
handle. Native event text preserves the full pointer; Win32 event text keeps its
defined old hexadecimal output. The real assertion UI/logger is not exercised.

Native `CThread::m_szName` grows from 32 to 64 bytes, guarded by `_WIN64` or
`__LP64__`, so both pointer identities fit without changing the returned member
buffer's identity or lifetime. **This is an explicit native CThread layout/ABI
change requiring all owned native consumers to rebuild.** Win32 keeps the old
32-byte member. Fixture layout output records actual compiler sizes, member
position and capacity; it does not infer Windows layouts from a macOS model.
`CThreadFastMutex` methods and storage remain unchanged by this task. Repeat
the Windows mutex matrix after changing this shared header.

Profiler serial storage stays `int` plus `CThreadLocal<int>` (an empty subclass of
`CThreadLocalBase`, whose API accepts opaque `void*`). Encoding explicitly retains
the unsigned 32-bit serial pattern in `uintptr_t`, then creates an opaque token.
Decoding retains its low 32 bits and compares against the same unsigned view of
the unchanged int counter. No token is dereferenced or treated as allocated
storage. Nonzero token high bits continue to be ignored for eligibility, matching
the old Win32-style comparison. The signed counter increment and its inherited
`INT_MAX` overflow limit are unchanged; tests do not execute that overflow.

Cases cover:

- Actual native class capacity/layout, a real high-address CThread object, a
  full-width opaque thread identity, exact diagnostic text, object guards, stable
  returned member pointer, cached/custom names, truncation and termination.
- Actual TLS round trips of zero, high serial bits and native full-width tokens;
  initial empty slots and isolation across a real `std::thread`.
- Canonical serial tokens for positive and negative int patterns, nested
  Start/Stop depth, same/mismatched/high-bits eligibility, child entry/exit,
  budget flags, disabled-at-root behavior and the existing non-thread-safe bypass.

Node timing/tree services and CThread lifecycle methods outside the diagnostic
boundary use controlled fixture bindings. No real worker, event or thread handle
is created, closed, stopped or joined through those fake identities. Timers,
profiler global aggregation/concurrency, whole translation units, tier0 DLL
integration and game/runtime behavior remain unqualified.

On macOS, seven cases use actual native headers/TLS and POSIX method bodies. Two
additional cases compile the actual Windows diagnostic bodies in a separately
named owned field context, using real host mutexes. They prove body-level format
and pointer behavior only, **not a Windows ABI, SDK or class-layout result**.
Actual Windows builds use the real Windows header/classes/API bodies for all
eight cases. The actual macOS `pthread_t` pointer/integer category is printed;
the production helper handles either platform representation without narrowing.

From a matching x86/x64/ARM64 MSVC prompt with Python, Ninja and the SDK:

```powershell
cmake -S tests/native/tier0_handles -B C:/CI/CSE-Tier0Handles/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64
cmake --build C:/CI/CSE-Tier0Handles/x64-Release --parallel
ctest --test-dir C:/CI/CSE-Tier0Handles/x64-Release --output-on-failure -V
```

Use separate directories and matching compiler prompts for x86/x64/ARM64 and
Release/Debug. Repeat the existing `tests/native/thread_mutex` matrix using this
final shared header. No new production source membership is required.

macOS ARM64 uses local `build/tier0-handles-macos-arm64-release`/`debug` directories
with `-DCSE_TARGET_ARCH=arm64`. A separate Debug directory can add
`-DCSE_TIER0_HANDLES_SANITIZERS=ON` for ASan/UBSan.

For a negative/reference build, copy the archived original
`tier0/threadtools.cpp`, `tier0/vprof.cpp` and `public/tier0/threadtools.h` into a
separate SDK-shaped directory and set `-DCSE_TIER0_HANDLES_SOURCE_DIR=...` plus
`-DCSE_TIER0_HANDLES_LEGACY=ON`. Visible truncation warnings remain enabled; Clang
MS extensions allow the old pointer casts to compile. The original native
capacity/name, event/body-format and canonical high-serial tests are expected RED.
Existing low-32 scope eligibility and TLS-isolation cases also run against the
original to establish their preserved behavior. On actual Win32, the malformed
name is expected RED; valid event output and serial/TLS behavior stay comparable.
