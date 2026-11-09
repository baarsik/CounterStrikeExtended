# Owned audio module lifecycle

The engine's local `AudioModuleShutdown` guard sits outside each retained
`CEngineAPI::Run` try block. It runs after normal return or caught C++/SEH failure,
while the engine DLL and its callbacks are still loaded. It joins the CD command
worker, closes the independently owned recorder and mixer controls, and repeats idempotent
music-player/native SDL effects cleanup. Normal `Host_Shutdown` ordering stays unchanged. It does not call
broad host shutdown from unknown partial initialization.

Full CD-controller shutdown can free track-list storage, so only the worker's
first controller-owning shutdown performs it. Repeated guard calls close only
empty capture/music/effects owners. A failed join or interrupted controller transition
must not return to a launcher that could unload running code: the guard logs to
stderr and terminates the process (`ERROR_INVALID_STATE` on Windows, `abort` on
POSIX). This fatal path does not pin the DLL, leak it deliberately, or terminate
individual workers during successful shutdown.

The private command worker checks synchronization and thread creation. Failed
initialization unwinds only resources already acquired and closes an initialized
controller once. A POSIX thread ID is stored only after successful creation.
Always-live access synchronization prevents late enqueues from reaching deleted
queue primitives. Shutdown rejects new work, cancels pending commands, lets the
current callback finish, joins the worker, and only then closes audio resources.
No lifecycle lock spans controller callbacks. If a C++ or asynchronous exception
interrupts a controller transition, the Run guard sees the unfinished transition
and fails closed rather than blocking on an abandoned lock.

## Actual component fixture

The loadable module compiles actual `Mpg123MusicPlayer`, `Mpg123Decoder`,
`SdlAudioBackend` and `CThread` sources. `prepare_lifecycle.py` extracts the exact
production module-cleanup helper/destructor, native `SNDDMA_Shutdown`, and hardware
`CEngineAPI::Run` bodies, recording source/body hashes. The surrounding game and
CD-controller callbacks are controlled. The test DMA object contains only the
members used by the extracted close body; it is not an ABI-layout fixture.

Actual pinned SDL3 3.4.20 and mpg123 1.33.7 images come from frozen provider/runtime
receipts. SDL's dummy output requires no physical playback device or microphone.
Windows retains `/EHa` and precise FP for the actual Run boundary body, `/EHsc`
for the CD cleanup/music/decoder bodies, and the worker's absent EH flag with
precise FP. The native DMA close body also keeps its absent EH flag. Separate
fixture TUs enforce these policies. No production EH/FP build policy is changed.

The cases exercise normal returns, caught C++/SEH exceptions, primed/active music,
music acquired without a command worker, initialization failure/rollback/retry,
pending and in-flight commands, rejected late enqueue, repeated cleanup, and
preservation of an external SDL audio reference. Successful cleanup must actually
remove the module and release only its own subsystem references. Join and
interrupted-transition faults must terminate before DLL unload.

`worker_fault_injection.*` wraps only OS calls in the actual worker translation
unit. It injects Windows mutex/event/thread failures and POSIX attribute/type,
mutex/condition/thread failures, plus join failure. Real successful calls and
resource counters verify rollback rather than substituting a fake worker.

The original read-only v2 diagnostic is preserved outside the checkout. Its old
catch-and-return path reproduced an effects destructor blocked inside
`FreeLibrary`; symbolic CDB showed CRT DLL detach → static effects destructor →
`SDL_DestroyAudioStream`. Active looping music instead left the module mapped
after `FreeLibrary` returned. The exact mechanism retaining that old module was
not established. The archive retains both kinds of negative evidence.

Microsoft explains the loader-lock synchronization restriction in its
[DLL best practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).
SDL documents that destroying an `OpenAudioDeviceStream` stream also closes its
device in [SDL_DestroyAudioStream](https://wiki.libsdl.org/SDL3/SDL_DestroyAudioStream).
The pinned local SDL source shows physical device closure joining the device
thread. This component changes neither provider.

This fixture does not execute the complete engine/client TUs, production CD
track allocation/free, physical CD/MCI, audible output or gameplay. It does not
fix the original CS MOTD failure. Production runtime acceptance remains separate.
