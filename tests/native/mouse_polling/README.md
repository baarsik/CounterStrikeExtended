This component qualifies native64 client input ownership and the Windows polling worker with controlled input services.
It does not open an SDL provider, query a real cursor, create a window, or qualify game/camera behavior.

`native_mouse_polling.h` is the internal C++ declaration shared with each client's camera code. The main thread passes
combined camera ownership after publishing its effective flags, before the first camera warp, and releases it after the
final recenter. Actual edges discard SDL pending motion without pumping events, fractional/integer accumulation and the
worker's queue/baseline. Repeated calls are idempotent. Filter history and button history retain their prior policies.
Deactivate and input Clear do not invent camera intent; Clear preserves its existing active-only main-state behavior.

`Poller<Backend>` owns one synchronized pair/baseline/generation, in-flight marker, eligibility/timeout snapshot and sticky
first failure. The worker does not read engine/HUD/VGUI/cvar/SDL state. Suppression retires old ownership and waits for a
pending query; ordinary recenter instead preserves queued motion and captures unpolled movement before the warp.
The owner starts requested workers parked even when raw input is initially enabled. Failed creation/read/control/timing
falls back to direct nonraw input only after cooperative cleanup; failures remain latched until a new Init. The existing
HL live raw cvar and CS initial `>0`, refresh `!=0`, strictly `>1s` cache policies remain distinct.

Production Windows synchronization uses SRW/CV, a manual-reset control event and the actual thread handle. Join occurs
outside the state lock, with a 5000ms bound per quiescence/join operation. Handles close after a successful join. A failed
shutdown cannot return to the legacy void HUD shutdown boundary: it terminates the current process, with fast-fail as a
fallback. The ordinary path does not terminate workers, pin modules, or defer cleanup to DLL detach. The Win32 selected
implementation remains unchanged, including its previously characterized unsafe worker behavior.

Microsoft's [condition-variable contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw)
requires predicate rechecking after wake and reacquires the lock. The backend uses
[thread/event waits](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject), with error
and timeout handled separately. The accepted fatal boundary uses the documented
[self-termination behavior](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess).

`core_tests.cpp` uses real owned threads and fake synchronization/input services. It tests whole-pair consumption,
queued and unpolled motion across ordinary recenter, late-query handoff, parked raw input, timeout publication and
conversion bounds, failure latching, stage rollback, finite quiescence/join failures, and repeated lifetimes.
`kernel_tests.cpp` uses actual Windows kernel threads/events with fake cursor reads and nine injected API failure paths.
`wiring_tests.cpp` includes complete current actual client functions/state extracted by the retained client-input fixture;
engine/HUD/SDL/Win32 cursor/SPI are controlled surrounding seams, and the Windows worker is real. It covers camera edges,
visibility/intermission/raw transitions, clear/deactivate, direct fallback and repeated shutdown.
`check_fatal.py` requires each actual held-worker shutdown child to exit 1 without continuing its caller.
`worker_module.cpp`/`module_tests.cpp` execute 21 actual load/start/stop/unload cycles and require no retained kernel handles.
They are owned fixture modules, not production client ABI qualification. Import checks reject SDL3 and User32 dependencies
from all fixture images. The production owning-TU gates are compile-only with the retained policies.

Mac controls use explicit `-O2` Release, `-O0 -g` Debug, precise/no-contraction floating point and ASan+UBSan; an additional
ThreadSanitizer run covers the generic owned-thread core. Windows controls cover x86/x64/ARM64 R/D; the native kernel,
actual native Windows body and DLL gates apply to x64/ARM64 only. Every CPU also runs the generic core and the retained
non-Windows body fixture. Four real Win32 owning-TU preprocessing comparisons require identical selected tokens.

Example component commands (the SDL path supplies pinned 3.4.20 headers only):

```
cmake -S tests/native/mouse_polling -B build/mouse-polling -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_SDL_INCLUDE=<include>
cmake --build build/mouse-polling --parallel 2
ctest --test-dir build/mouse-polling --output-on-failure
```

`run_negative_variants.py` rejects seven actual-core mutations. `run_body_negative.py` rejects ten complete actual-body
handoff mutations. Earlier reference/failure packets remain separate and unchanged. Integration retains the three new
common headers for both client owners; no new production TU, callback-table slot, link dependency or provider is added.
Actual camera API10 integration, physical input/window behavior, and full runtime acceptance remain separate gates.

Native64 Windows direct nonraw reads now use a local POINT and commit it only after GetCursorPos succeeds. Failed
MouseMove/Accumulate defers the sample, preserves prior accumulation/scaled/filter/button/command state and does not
warp. A later successful sample consumes valid prior accumulation once. CS performs only its exact strict >1s,
refreshed !=0 cache update on read failure; ordinary Reset retains warp-before-refresh order and startup remains >0.
`--direct-read-safety` exercises unchanged/poisoned failure outputs, direct/post-worker-fault modes, retry and CS recovery.
The additional native Windows cases bring the six-profile matrix to 50 cases; the production Win32 branch stays intact.

Failed direct SetCursorPos remains a separate preexisting limit: without a successful recenter it can count an unchanged
off-center position again. The threaded path reads and rebases the actual post-warp position. These controls do not
claim that direct failed-warp handling, real focus/DPI/input behavior, or exceptional unload skipping IN_Shutdown is
qualified. V1 frozen source and receipts remain unchanged; the V2 packet records the bounded direct-read repair.
