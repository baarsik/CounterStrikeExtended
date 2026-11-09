This fixture executes the whole production `tier0/testthread.cpp` translation
unit. It does not reconstruct the harness in a test implementation, link the
rest of tier0, or execute a game/provider/UI/network. The owning generated tier0
compiler row is replayed, including its headers and configuration flags. The
fixture main compiles as C++14; a separate whole-source C++14 compile profile is
recorded alongside the actual owning C++17 profile.

The approved native contract preserves the Windows API state/order intent:

| Operation/state | Native contract |
| --- | --- |
| Initial | Inactive, unfinished, not failed. |
| Successful RunTest | Active stays true thereafter; callback parks before its first grant. |
| RunFrame | Controlling thread grants one continuation and waits for that grant's next yield or completion. |
| Yield | Worker publishes a new yield generation, then parks for a grant or cooperative stop. |
| Completion/stop | Finished means active and callback no longer running; join ownership remains until reaped. |
| Failure | Failed is sticky, including calls from dbg.cpp assertion/error reporting. |
| Startup failure | No worker/handle is fabricated; failed is set; prior sticky active state is retained. |
| Stop/destruction | Signal stop, wait for callback cooperation and join outside the state mutex before destroying storage. |

All eight C exports and the callback/void-pointer signatures remain. Callbacks
and their arguments are caller-owned; storage needed by callback/TLS cleanup must
remain alive through join. Native worker handles and controlling identity remain
private pthread_t values, compared with pthread_equal. No pointer is truncated
into an integer ID. There is one mutex for all state, predicate loops for spurious
wakeups and a generation counter that prevents stale startup notifications from
satisfying a later frame. The owned worker handle is not cleared until join.

A worker cooperatively exits itself at Yield via pthread_exit, with the proxy's
pthread cleanup handler completing state and waking the controller. There is no
pthread_cancel, detach or termination of another thread. Callbacks must eventually
yield or return: an uncooperative callback can block RunFrame/shutdown. There is no
invented production deadline or unsafe timeout fallback. No promise is made about
portable C++ automatic-object unwinding beyond the pthread cleanup contract.
Invalid owner/reentrant operations set failure and return; destroying the harness
from its own worker or concurrently with an in-progress join is rejected fatally,
as its live storage cannot be abandoned safely. Public use after destruction is
outside the lifetime contract.

The retained Windows source has different inherited defects: stale initial
main-event credit, lost natural-completion HANDLE and failed-create running state.
Root explicitly approved the predicate/generation native contract instead of
emulating those bugs. The POSIX rewrite also fixes the actual pthread_create
argument (harness pointer, not address of an unassigned scalar ID), zero-assigned
IDs, unsynchronized flags, unpredicated waits, cancellation and incomplete cleanup.
The entire Windows-selected source remains byte-for-byte identical to frozen V12.

Only Test_SetFailed is declared in retained public headers. A complete source
search finds its consumer in dbg.cpp::_SpewMessageType for assert/error reporting;
no retained callers of the other Test_* exports were found. This does not establish
that arbitrary external callers comply with the cooperative contract.

Nine scenarios cover initial state, 100 full-width-argument run/yield/completion
cycles, initial/parked stop, transactional EAGAIN startup failure and restart,
sticky failure, retry of a failed join, invalid owner/reentrant calls without
replacing the callback, concurrent status/failure reporting, actual global-exit
join, TLS-destructor failure reporting during join, and 50 spurious-wakeup cycles.

The source is compiled with typed pthread_create/pthread_join/pthread_cond_wait
bindings. Normally they call the real APIs. Controls inject one EAGAIN, one join
EINVAL and legal spurious condition returns; no fake worker is reported started.
The successful-join marker is written only after real pthread_join returns zero.
The parent process reads it after the child exits, so global-destruction cleanup
is measured after main, not merely asserted before teardown. The TLS scenario
holds worker TLS cleanup until the real join begins, then calls Test_SetFailed;
join must release the harness mutex to avoid a real deadlock.

Fixture predicate waits use monotonic absolute two-second deadlines. Each child
has an eight-second monotonic process deadline and CTest adds a twelve-second
outer limit. These limits apply only to the isolated fixture, not production.
Process timeout is a failure, not a passing stop policy.

```sh
cmake -S tests/native/test_thread -B build/test-thread-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCSE_TEST_THREAD_DATABASE=/absolute/path/to/actual/Release/compile_commands.json
cmake --build build/test-thread-release
ctest --test-dir build/test-thread-release --output-on-failure -V
```

Use the actual Debug database for Debug; add CSE_TEST_THREAD_SANITIZER=address for
ASan+UBSan or =thread for TSan. Both the whole production TU and fixture receive
instrumentation, with no sanitizer suppression. No LeakSanitizer claim is made.
The immutable evidence also retains the two actual original whole-TU compiler
errors and two narrowly labeled historical-body controls: exactly two pthread_t
field type shims permit compilation, then a typed create control observes the
wrong argument or EAGAIN exposes broken startup state. Those controls are not
claims that the original unmodified native source compiled or executed safely.

No production membership/link change is required. This gate covers the native
harness component, not all tier0 consumers, arbitrary callback lifetime behavior,
module shutdown with concurrent API callers or full native game acceptance.
