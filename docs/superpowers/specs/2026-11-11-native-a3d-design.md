# Native A3D: retained software and API design

Status: canonical design for the controller-selected software/API/module port, 2026-11-11. Implementation has not started under this document. The user opted out of further workflow questions; the controller selected the approach below. `dedicated_host` remains the sole production implementer until the controller hands off ownership. This design authorizes no concurrent edits or runtime work by its author.

## Outcome and boundaries

Port the real retained `a3dapi` responsibility to macOS 14+ ARM64, preserving its owned software algorithms, public interface families, geometry/source producers and supported software behavior. Supply actual native device, buffer, allocation, scheduling and shutdown ownership. Keep the default engine mixer unchanged. Preserve the Windows OEM/legacy routes and native Windows layouts.

The retained software A2D control consumer is the reference for software behavior. Its observed omission of EQ/reflection rendering is not a migration regression to repair with invented DSP. Preserve the geometry/reflection producers independently, and report unsupported rendering and unknown OEM behavior honestly. Native software module acceptance can finish without claiming full vendor feature parity or an unproven game consumer. Broader migration acceptance remains open wherever those larger requirements are unqualified.

No fake Windows API success layer, `E_NOTIMPL` wrapper counted as playback, fabricated OEM identity, disabled target, new engine audio bridge, speculative HRTF/reverb/EQ algorithm, or replacement of the default mixer is selected. A working exported factory is necessary but insufficient; the actual module must render supported PCM through a real native output owner and cleanly release it.

All 19 retained responsibilities remain: `vgui`, `hw_engine`, `tier0`, `vgui2`, `filesystem_stdio`, `launcher`, `cs_client`, `cs_server`, `hl_client`, `hl_server`, `gameui`, `particleman`, `hltv_core`, `demoplayer`, `hlds`, `swds`, `hltv`, `hltv_proxy`, `a3dapi`. CEF helpers are additional dependencies. This document owns A3D only.

## Evidence and corrected reference contract

Read the preserved findings and controller checks in `build/macos-runtime-bringup-20261109/a3d-reference-analysis-v1/`. The exact copied stock `A3dApi.dll` SHA256 is `f05b2eeb851b174ef2b39c4728687915648ae33780a65cdf7f0c7ce99e6a67af`. The copied-byte provenance is in sibling `a3d-reference-inventory-v1/`. This is existing static analysis evidence, not a runtime/provider experiment performed for this design.

The complete stock control body `0x10001600..0x1000173a`, selected through the independently checked A2D DAL vtable, reads control offsets `0x14, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38`: pitch and left/right azimuth, elevation, gain and delay. It returns success. It does not read source `flEq` at `0x10` or reflection records at `0x4c` onward. Retained `A2DBuffer::SetA3dSuperCtrl` has the same inputs and operations. Evidence: `ev_a11efb02f65f5ee952b70d4d3d1808432b51f6636f7f75dc3525badff0ec2037`; complete assembly is retained because x87 decompilation omits arithmetic arguments. No sample-for-sample comparison of the entire shipped DSP is implied.

| Boundary | Required claim/test | Claim it does not establish |
| --- | --- | --- |
| Geometry/source producers | Preserve transformations, materials, occlusion/distance/cone/doppler math, reflection generation/ordering/slot history and complete control records | All produced fields affect software PCM |
| Stock software control consumer | Preserve direct pitch/gain/delay/HRTF state; changing only unread EQ/reflection fields leaves consumer state/PCM unchanged and retains observed success | Audible reflections, EQ filtering or OEM parity |
| Native software capability API | Advertise only qualified rendering; unsupported public feature requests fail without partial initialization | Success from the low-level control method means all record fields were rendered |
| Device transport | Real SDL acquisition, queued supported PCM, measured playback and deterministic close | Equality of physical-device conversion or spatial perception on every device |
| OEM boundary | Preserve Windows activation; record actual missing-provider or genuine-provider observations | Mac software simulates an unavailable OEM driver |
| Game consumer | Actual identified module/API/call path and runtime evidence | Game startup or coexistence with an unused A3D fixture is integration |

This supersedes the old draft requirement that every EQ/reflection control mutation must change software PCM. It also supersedes blanket rejection of a low-level record merely because stock-unread fields are populated. It does not convert unknown methods into no-ops: only specifically evidenced compatibility behavior receives that treatment.

The stock factory still activates OEM CLSID `442d12a1-2641-11d2-90fb-006008a1f441`, then legacy COM fallback. The targeted existing inventory did not find that provider; inventory coverage is bounded. OEM EQ/reflection semantics remain unknown under `unk_56dd5423606b49334c621113d59cbf39d7ecae57cdbb379cab90a2ca00023bbf`.

The retained engine uses `snd_dma.cpp`/`snd_mix.cpp`, native `snd_sdl.cpp`, and Windows `snd_win.cpp` loading `dsound.dll`/`DirectSoundCreate`. Inspected historical music code uses AIL stream/digital APIs. Existing stock `hw.dll` analysis found 24 import DLLs/650 imports, 16 Miles stream/digital operations, no direct A3D/OLE32 import, and dynamic lookup primitives. Static imports and source searches do not disprove every dynamic consumer. No actual engine A3D consumer is established, so no bridge is selected.

## Approaches

**Selected by the controller: native retained software/API/module.** Keep the module and owned algorithms, replace platform ownership, qualify the demonstrated software behavior, and preserve unsupported/OEM/game-consumer gaps explicitly. This permits useful complete software acceptance without inventing vendor algorithms.

**Not selected: extend A2D with new reflection/EQ/HRTF DSP.** That changes the observed software behavior and requires its own evidence and design. Producers alone do not specify the absent vendor transfer function.

**Not selected: engine bridge or Windows OEM helper.** A new bridge would alter channel/timeline/world-material behavior without an established consumer. A helper depends on a real Windows provider/hardware and is not a native Mac software implementation. Genuine provider observations may later supplement reference evidence, but are not prerequisites for declaration/ownership/software parity work.

## Current owning gaps

- `A3d3.cpp` owns exports/factory/Windows version checks. `resman.cpp::InitResMan` independently activates the OEM backend. Both native construction paths need explicit owned injection; replacing only the factory leaves the second activation unresolved.
- `native_device_init.h` already defines typed `IA3dNativeDeviceV1::InitializeDevice(const GUID*, DWORD, DWORD, DWORD*)`, IID `5951edd2-e13f-506b-b34e-e0336d864df8`. The GUID is borrowed during the call and copied if retained. Keep the old `IA3dDal::InitializeEx(DWORD, DWORD, void*, void*)` slot untouched; no pointer fits into that scalar lane on a native process.
- `CA3dSoundBackend` initialization, creation/duplication, speaker/output/resource methods, caps and many other operations return `E_NOTIMPL`. Some buffer/mapper getters return success without outputs. Audit each callable operation; implement real operations required by the native route, preserve evidenced compatibility semantics, and fail remaining unsupported operations honestly.
- `A3dSource.cpp::AllocateWaveData` needs actual `IDirectSoundBuffer` and `IDirectSound3DBuffer`, then optional A3D render control. A short test interface or PCM-only object cannot satisfy it.
- `ResManBuffer::SetA3dSuperCtrl` stores a borrowed pointer and discards the downstream HRESULT. Copy queued control state and propagate actual failure on the native route. The observed stock A2D success for ignored fields remains success; malformed input or actual owner/device failure is different.
- `ResMan::RenderThread` and `DAL_A2D::ThreadProc` have unbounded loops; `DAL_A2D::Init` starts the worker before HRTF initialization and failure cleanup can race it. Concrete native owners need stop/wake/join, retained source/PCM owners and complete partial-init unwind.
- `DalInfo` derives software pool capacity from hardware-named caps. Use a private native capacity interface for native pools; hardware caps must not be populated with fabricated hardware resources. Preserve the external Windows provider contract.
- `a2d.cpp::Mixer_FloatToPcm16` uses host `long`/`LONG_MIN/MAX`. The component Mac adapter pins Windows bounds, so production LP64 conversion still needs exact signed-32-bit conversion/overflow tests. Existing 32 PCM receipts and padded fixtures are bounded evidence, not all-input safety.

## Architecture and ownership

Paths below are relative to `GoldSrc-SDK/` unless otherwise noted. All new names are proposed owned implementation units, not existing exported interfaces.

| Owner/files | Producer/consumer boundary |
| --- | --- |
| `a3dapi/platform_types.h`, `platform_com.h`, `platform_dsound.h` | Windows uses actual SDK declarations; Mac gets explicit fixed-width scalar/GUID/record declarations and complete consumed interface families. Headers contain no fake OS service implementations |
| `a3dapi/native_platform.h/.cpp` | Monotonic clock, copied wave-file data, allocations and worker ownership; platform calls return real outcomes |
| `public/cse_a3d_notifications.h`, `a3dapi/native_notifications.h/.cpp` | Versioned native event/notification ABI, same-module event admission, wait/close state, copied registrations and fenced internal dispatch; exact contract below |
| `a3dapi/native_buffer.h/.cpp`, `native_mix.h/.cpp` | Concrete native buffers reuse retained A2D control/kernel/table operations; immutable copied format/control state feeds one serialized render owner; no new acoustic algorithm |
| `a3dapi/native_output.h/.cpp` | Actual SDL stream and bounded prepared-PCM queue; callback only drains PCM, render owner does all mutable list/DSP work |
| `a3dapi/native_provider.h/.cpp` | Concrete native software facade implements existing DirectSound/DAL/A3D views plus native initialization; constructs a distinct leaf software renderer, then injects it into resource ownership |
| `a3dapi/resman.h/.cpp`, `dalinfo.h/.cpp`, `dal_a2d.h/.cpp` | Existing scheduling/order/pool responsibilities preserved; new nonvirtual native attach/shutdown seams accept an initialized leaf renderer, not the facade factory, preventing recursive activation |
| `a3dapi/native_capacity.h` | Separately versioned private `IA3dNativeCapacityV1`, `GetVoiceCapacity(DWORD* total, DWORD* available)`, with distinct UUIDv5 name `CounterStrikeExtended/IA3dNativeCapacityV1`; public caps semantics stay separate |
| `public/cse_a3d_abi.h`, `a3dapi/native_exports.cpp` | Native role/layout/interface admission through existing `CseGetModuleAbi` convention, before invoking `DllGetClassObject` or exchanging a virtual view |

Keep Windows class storage/layout unchanged: native-specific state is platform-conditioned private storage or a new concrete native owner, never unconditional extra bases/slots or a virtual destructor in an existing ABI interface. Preserve all original native Windows and Win32 slot orders and scalar layouts. Same-width Itanium and Microsoft C++ objects are not interchangeable; consumers and providers are rebuilt together and checked before use.

Native provider construction returns one owned `IUnknown` reference with a canonical identity across views; failure clears the output. The provider owns output/render state and retains resource children until quiescent. Held source/buffer interfaces keep necessary state and code alive after the external parent reference is released. Never delete an externally retained source merely to make unload return success.

The render owner serializes mutable voice/pool/control work across instances that share old static mixer scratch. Prefer owner-scoped scratch only if extraction preserves source arithmetic and policies; otherwise serialize the actual shared kernel with no cross-instance race. The callback never walks COM/source lists, allocates geometry, invokes external notifications under a lock, waits for workers, or unloads modules.

Shutdown order: stop new work, signal and join producers, pause/destroy SDL stream with callback quiescence, retire notification/control queues, release buffers/pools/provider references, then permit module unload. Partial failures use the same cleanup. Format descriptors, GUIDs and control records are copied; DWORD cursor/tick projections use explicit bounded arithmetic from wider internal storage.

## Public capability policy and preserved software behavior

The native software profile is explicitly named **retained A2D software**. The initial qualification candidate is `A3D_DIRECT_PATH_A3D` (`0x08`, the current public definition), admitted only after task 6 verifies its actual producer-to-control-to-PCM path. This design is not that qualification. `A3D_DISABLE_SPLASHSCREEN` (`0x80`) is an initialization option, not a rendering capability; accepting it requires the actual no-splash native behavior. A zero rendering request may initialize a qualified direct profile. Reject requested unqualified rendering bits before opening/committing state, with `A3DERROR_FEATURE_NOT_SUPPORTED` and zero `supportedFeatures`; do not replace them with silent success. `A3D_1ST_REFLECTIONS` (`0x02`) is not advertised by this software profile.

`A3D_OCCLUSIONS` (`0x40`) and `A3D_DIRECT_PATH_GENERIC` (`0x20`) remain unqualified public rendering claims until their complete API semantics and concrete native route are tested. This conservative native admission does not remove the retained producer code or alter the legacy Windows route. The producer's direct occlusion gain may affect the consumed gains, while its EQ field remains ignored by stock A2D; do not call the entire source computation absent. The controller may expand the native mask only with the precise task-6 evidence, never simply by restoring an unconditional OR.

`CA3d::Init` currently ORs occlusion/direct bits unconditionally and ignores resource-mode failure. Native-owned initialization must use the negotiated mask and real failure result; scope this to the owned native provider rather than changing external Windows compatibility behavior. Preserve the existing HRESULT compatibility mapping for other errors unless a specifically tested native contract requires a new result. Feature availability/Enable must reject unsupported native bit combinations, not succeed because any one bit overlaps.

At the low-level software control boundary, a valid complete record is consumed exactly as the evidenced A2D routine: pitch and two direct-channel tuples affect state; modifying only unread fields leaves it unchanged and returns the observed success. This success is acceptance of that control record under the named software profile, not a claim that reflections/EQ rendered. Test it explicitly. Unknown unsupported API operations do not inherit this exception.

Geometry/source producers remain in the module and retain their full records, algorithms and identity. Tests can exercise exact producer bodies/state with a labeled sink that captures records, separately from the public native rendering feature mask. Their outputs and software-control behavior are distinct required regressions.

## Native notification contract (builder refinement)

The controller selected a real native counterpart, not Windows-handle emulation. On macOS, legacy `IID_IDirectSoundNotify` queries fail `E_NOINTERFACE` with a null output; any reachable legacy `SetNotificationPositions(DWORD, LPCDSBPOSITIONNOTIFY)` slot fails `E_NOTIMPL` without dereferencing the supplied handles. A creation request containing `DSBCAPS_CTRLPOSITIONNOTIFY` fails `DSERR_UNSUPPORTED`; standard buffer caps do not advertise that legacy flag. The separately negotiated typed view below is available without that flag. Keep Windows queries, handles, records, slot order and behavior unchanged. Merely rejecting the legacy view does not satisfy native notification acceptance.

Source evidence is deliberately narrower than the new platform contract. `resman.cpp::ResManBuffer::Update` compares `[previousReadPosition, newReadPosition)` after stream `Write`; this is submitted/fill progress. `dal_a2d.cpp::ResManStatBuffer::ProcessNotifications` compares the same half-open interval using its read cursor. Both call `SetEvent` under a notification mutex. `ResManBuffer::SetNotificationPositions` borrows the array, ignores a later nonempty replacement when the mutex exists, and clears on count zero. These bodies do not explicitly handle wrapped intervals or the stop sentinel. `CA3dLegacySoundBuffer::SetNotificationPositions` returns `E_NOTIMPL`. Native copy/retention, wrap, stop and replacement semantics below are the explicit platform counterpart, not a claim that these legacy bodies already implement them. Timing-only EMU views do not acquire notification support merely through inheritance.

### Public declarations and admission

Define these exact new declarations in `GoldSrc-SDK/public/cse_a3d_notifications.h`; `DWORD`, `HRESULT`, `IUnknown`, `STDMETHODCALLTYPE` come from the admitted platform declarations, and `CSE_ABI_CALL` from `cse_module_abi.h`. They are independent interfaces with the three inherited IUnknown slots and the listed methods in order, with no virtual destructor or added legacy slots. Export the factory from the native Mac module; no new Windows legacy export requirement is imposed.

```cpp
constexpr DWORD A3D_NATIVE_NOTIFICATION_VERSION = 1;
constexpr DWORD A3D_NATIVE_EVENT_AUTO_RESET = 0;
constexpr DWORD A3D_NATIVE_EVENT_MANUAL_RESET = 1;
constexpr DWORD A3D_NATIVE_WAIT_INFINITE = 0xffffffffu;
constexpr DWORD A3D_NATIVE_NOTIFICATION_STOP = 0xffffffffu;
constexpr DWORD A3D_NATIVE_NOTIFICATION_MAX_POSITIONS = 1024;
constexpr DWORD A3D_NATIVE_NOTIFY_BYTE_OFFSETS = 0x01;
constexpr DWORD A3D_NATIVE_NOTIFY_STOP = 0x02;
constexpr DWORD A3D_NATIVE_NOTIFY_WRAP = 0x04;
constexpr DWORD A3D_NATIVE_NOTIFY_ATOMIC_REPLACE = 0x08;
constexpr DWORD A3D_NATIVE_NOTIFY_RETAINED_EVENTS = 0x10;
constexpr DWORD A3D_NATIVE_CURSOR_RENDER_CONSUMED = 1;
constexpr DWORD A3D_NATIVE_CURSOR_STREAM_SUBMITTED = 2;

enum class A3dNativeWaitStatusV1 : std::uint32_t {
    Invalid = 0,
    Signaled = 1,
    Timeout = 2,
    Cancelled = 3,
    Closed = 4
};

struct A3dNativeEventDescV1 {
    DWORD structureBytes;
    DWORD version;
    DWORD resetMode;
    DWORD initiallySignaled;
};

struct IA3dNativeEventV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Signal() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE Wait(DWORD timeoutMilliseconds,
        IA3dNativeEventV1* cancellation, A3dNativeWaitStatusV1* result) = 0;
};

struct A3dNativeNotificationPositionV1 {
    DWORD offsetBytes;
    DWORD reserved;
    IA3dNativeEventV1* event;
};

struct A3dNativeNotificationCapsV1 {
    DWORD structureBytes;
    DWORD version;
    DWORD flags;
    DWORD maximumPositions;
    DWORD cursorKind;
    DWORD reserved[3];
};

struct IA3dNativeNotifyV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetNotificationCaps(A3dNativeNotificationCapsV1* caps) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetNotificationPositions(DWORD count,
        const A3dNativeNotificationPositionV1* positions) = 0;
};

extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeEventV1(
    const A3dNativeEventDescV1* description, IA3dNativeEventV1** event);
```

Pin `IID_IA3dNativeEventV1` to `ec05e92f-4c19-571e-9ee4-d172a61b0c3a` and `IID_IA3dNativeNotifyV1` to `37efe390-0647-576f-8f4d-bbfe415acc81`, UUIDv5 URL-namespace names `CounterStrikeExtended/IA3dNativeEventV1` and `CounterStrikeExtended/IA3dNativeNotifyV1`. Both IIDs have canonical IUnknown identity on their respective concrete event/buffer owner. A wrapper exposing the notification view must retain its own canonical buffer identity rather than returning an inner object's unrelated identity.

The caller must first admit `CseGetModuleAbi` and the A3D extension descriptor: factory calling convention/signature, interface revisions/IIDs/slot signatures, fixed scalar sizes, all record sizes/offsets and notification semantics version. The event descriptor is 16 bytes; caps is 32; native-64 position is 16 with event at offset 8. These native-pointer records are not packed to Win32 sizes or transmitted as scalar records. A supported buffer's typed `QueryInterface` returns its own `IA3dNativeNotifyV1`; unsupported/timing-only views return `E_NOINTERFACE`, null output. Do not change acoustic `A3D_*` feature bits to advertise event support.

Factory inputs require exact descriptor size, version 1, reset mode 0/1 and initiallySignaled 0/1. Null description/output is `E_POINTER`; other malformed values `E_INVALIDARG`; output is cleared on every failure when writable. Success returns one owned event reference and needs no audio device/provider. `GetNotificationCaps` returns `E_POINTER` for null and otherwise requires a 32-byte version-1 record initialized by the caller; malformed size/version fails `E_INVALIDARG`. Success writes every field, zeroes reserved words, reports maximumPositions 1024 and cursor kind, and reports flags `0x1f` only after every defined behavior is implemented and tested. Failure preserves size/version but zeroes remaining fields when the valid-sized record is writable. This limit is a new native resource bound, not a historical limit. No partial capability profile is accepted as the completed V1 counterpart.

Only a live event produced by this factory in the same admitted loaded module instance is accepted for registration/cancellation. A module-private non-owning registry uses full-address concrete event identity and a generation, resolves it to retained internal state without invoking a caller-supplied virtual method, and removes it on final destruction. A same-IID foreign object or a different loaded module's event fails `E_INVALIDARG`; an IID alone is not admission. No pointer/identity enters a DWORD or HANDLE. The caller holds a valid reference for the duration of each API call; dangling pointer misuse is outside the contract. Success retains target state independently of caller references. Arbitrary external QueryInterface/AddRef/Release/callback code is never invoked under provider, registration or event locks.

### Event, wait and close semantics

- Auto reset is one sticky signal permit: Signal releases one eligible waiter or stores one permit; a successful Signaled wait consumes it. Repeated Signal while a permit is pending coalesces. Waiter order/fairness is unspecified. It is not a counter/semaphore.
- Manual reset is sticky: Signal completes all currently eligible waits and remains signaled for later waits until Reset. Reset clears an unconsumed signal; it cannot retract a completed wait result. No pulse operation is supplied. Signal/Reset on an open event returns `S_OK`.
- Wait timeout is monotonic milliseconds: 0 polls and `0xffffffff` waits indefinitely. Optional cancellation must be a distinct, same-module, manual-reset event; self/auto-reset/foreign cancellation fails `E_INVALIDARG`. A signaled or closed cancellation event yields Cancelled without consuming/resetting it. No OS handle is accepted.
- Wait initializes a writable result to Invalid. Null result fails `E_POINTER`. Normal completion returns `S_OK` with exactly Signaled, Timeout, Cancelled or Closed; actual internal resource/operation failures return an appropriate failure HRESULT, never a fabricated normal status. Pin the concrete target/cancellation objects and their state plus the module's active-call lease until the call exits; registering the wait and its cancellation wakeup must not lose a signal/close race.
- Close is terminal and idempotent (`S_OK`), clears a pending signal and wakes all unresolved target waits with Closed. A new Wait on a closed target returns `S_OK`/Closed. Signal/Reset on a closed event returns `E_ABORT`. Close does not release the caller's reference. Closing a cancellation event wakes its dependent waits as Cancelled, distinct from closing their target.
- For simultaneous conditions observed while deciding an unresolved wait, precedence is target Closed, cancellation signaled/closed, target signaled, then timeout. A previously committed outcome stays committed. A cancelled wait does not consume an auto-reset target's permit. Implement real condition-variable/platform wait wakeups rather than a polling/sleep loop.

Each event holds no provider/buffer reference. Each registration holds event state; each active wait holds target/cancellation state; none creates a provider cycle. A returned event reference, retained registration, active wait or in-flight factory/event operation keeps the module live. `DllCanUnloadNow` remains `S_FALSE` until these and existing object/worker counts are zero. The host keeps its library lease until admitted unload; the ABI does not promise safety if a hostile host force-unloads executing code. Provider shutdown does not close caller-owned shared events. Events remain usable after provider release; a caller cancels/closes a remaining infinite wait before graceful unload.

### Buffer registrations and crossing semantics

`SetNotificationPositions(0, nullptr)` removes all positions; for any zero count the positions pointer is ignored. A nonzero count requires a nonnull array, count at most 1024, zero reserved words, a same-module live open event and an offset less than buffer byte size or the native STOP sentinel. Any byte offset is valid, not only a sample boundary. Reject a zero-size/non-notifying buffer rather than manufacturing progress. Invalid inputs return `E_INVALIDARG` (`E_POINTER` for a nonzero count with a null array); an already-closed target returns `E_ABORT`; allocation failure returns `E_OUTOFMEMORY`. All failures preserve the old registration set. Caller array/event inputs are borrowed for the call; copy the array and retain every target on success. Duplicate offsets/targets are allowed and event semantics coalesce their effects.

Replacement/removal is atomic with a generation fence. It validates and acquires the entire new set before publication; old dispatch that has not committed is cancelled or joined, so **after return no old-generation signal can commit**. Already committed signals remain the event's state and are not reset by replacement/removal. Do not wait for dispatch while holding a lock that dispatch needs. A target closed after admission remains safely retained, but subsequent internal signals are dropped without failing playback; replacing/removing releases that retained target. A close racing validation may linearize before admission (reject) or after (admit then drop); there is no use-after-free or partial set.

The owner tracks unwrapped logical source-byte advances. A positive advance tests `[oldPosition, oldPosition + advance)` projected onto the buffer; split wrap into tail/head and recognize full/multiple laps. The ending byte is excluded until progress leaves it. Zero advance emits none. Play from byte zero therefore notifies offset zero on the first positive advance, not at Play itself. Seek resets the crossing baseline without retrospective notifications. Preserve producer timing domains: static/software buffers report `RENDER_CONSUMED` when the real mixer consumes source samples; resource streams report `STREAM_SUBMITTED` when the retained stream-write progression advances. Neither promises DAC time or audible device presentation. An implementation must expose the correct kind at the wrapper actually queried, not forward a leaf's incompatible kind. A changing resource mode must retain that exposed cursor meaning or reject the transition; it cannot silently change it under an existing registration.

STOP notifies once per playing-to-stopped transition: explicit Stop, natural nonlooping end, or orderly shutdown of a playing buffer. Already stopped/paused/seek/zero-progress updates do not generate STOP. If an update reaches offsets and then stops, process offset matches before STOP, coalescing signals to the same event within that update. Restart begins a new transition cycle. The stop sentinel is a notification kind, not a valid byte index. Looping advancement alone never signals STOP.

Compute crossings and pin internal target snapshots under the owning buffer/render lock, then leave that lock before dispatch. Internal dispatch may briefly lock its private event state; it calls no external interface or user callback. A waiter can immediately re-enter GetCurrentPosition, replace registrations, Stop or Close without a lock cycle with notification dispatch; ordinary concurrent buffer operations may briefly contend on their own lock. Fence generation and dispatch consistently so this re-entry cannot deadlock replacement. Callback/audio-device threads drain prepared PCM only; they do not perform notification waits or invoke external code.

Shutdown stops new registrations/playback, commits the one final STOP for each still-playing registered buffer, fences dispatch and drops registrations before retiring buffer workers. It does not close/reset the shared caller events. Pending offset-only waits consequently require caller cancellation/close; they intentionally keep unload busy. Final release of an unreferenced buffer uses the same stop/fence/drop behavior without resurrecting it. Failure unwind drops acquired state exactly once and wakes internal workers, never fabricating a position/STOP that playback did not reach.

### Native notification acceptance boundary

Acceptance requires actual production event and buffer owners, copied arrays, real native waits and all V1 semantics; rejection of HANDLEs by itself is insufficient. Independent caller declarations must exercise the actual loaded module's factory, QI/canonical identity and busy-unload contract. CPU fixtures must cover auto/manual resets, cancellation/close races, bounded/infinite waits, wrap/multiple laps, exact endpoints, STOP, replace/remove fences, source-array destruction, caller-reference release, target close and waiter re-entry. Use deterministic barriers and bounded watchdogs; no timing guess is evidence. Native semantics, standard Windows legacy view, stream-fill versus render-consumed cursor, and unavailable timing-only/OEM behavior are reported separately. No new engine consumer is introduced to obtain this test coverage.

## Numerical, layout and build contract

- macOS 14+ ARM64, C++17, CMake 3.25+, already pinned SDL3 3.4.20. No dependency installation is needed for design preparation.
- All 21 retained TUs stay selected; new owners are added through `GoldSrc-SDK/projects/vs2026/a3dapi.vcxproj` or the owning translator, followed by `python3 tools/translate_msbuild.py`. Never independently edit generated target files.
- Preserve per-TU FP/EH/optimization/CRT/assertion/diagnostic policy. Windows runtime remains `/MTd` Debug and `/MT` Release for the module; fixture-host `/MD` choices never leak into it. Retain level-4 warnings and warnings-as-errors, the generated native diagnostic set, strict/precise/fast distinctions, no pointer truncation suppression, and portable OS-termination policy for fatal faults. Do not copy all code into one differently configured TU.
- Global compiler concurrency cap is **2**. The future A3D implementation owns **1** compiler slot unless the controller explicitly allocates more within that cap. Use `--parallel 1` and `ctest --parallel 1`; no background duplicate build or nested unbounded compiler job. The current design task runs none.
- Keep `BYTE/WORD/DWORD/LONG/ULONG/HRESULT/BOOL` at 8/16/32/32/32/32/32 bits with correct signedness; GUID size 16 and field offsets fixed. WAVE packing is explicit and local. Native pointers remain full width; pointer-bearing objects are not forced into Win32 object sizes.
- Scalar-only sizes: channel control 16, reflection control 56, `A3DCTRL_SRC_SUPER` 1036 (`Reflections` offset 76, reserved backend offset 972), software/hardware caps 40/36. Confirm against independent Windows receipts. `A3dSourceReflection` is a different pointer-bearing record.
- Preserve `native_geometry_identity.h` full-address/domain identity, existing public DWORD tags and first-member reflection ownership. No recycled 32-bit pointer IDs or native pointers in reserved public scalar fields.
- Retain A2D 22050-Hz stereo S16, 8192-byte output buffer, 4096-byte low-water, 20-ms schedule and maximum 4096-frame kernel calls. Preserve ResMan's distinct 44100-Hz/88200-byte stream domain and 10-ms/100-ms scheduling. Do not silently merge rates or call filters omitted by the actual A2D loop.

The complete 21-TU inventory and per-file policy table follows; generated commands remain authoritative for defaults.

| TU | Owner | Explicit manifest policy beyond target defaults |
| --- | --- | --- |
| `A3d3.cpp` | Factory/objects/version | Defaults |
| `a3d_guids.cpp` | GUID definitions | Defaults |
| `resman.cpp` | Resources/streams | Defaults |
| `dal_a2d.cpp` | Software/timing DAL | Sync EH |
| `dalinfo.cpp` | Pool/stream/PCM | Sync EH |
| `A3dGeom.cpp` CORE | Geometry/commands/matrices | Precise, NoExtensions |
| `A3dScene.cpp` | Scene/device/flush | Precise, NoExtensions, Sync EH |
| `apimapper.cpp` | Adapter/commands | Strict, NoExtensions |
| `A3dSource.cpp` | Source/wave/buffer | Strict, NoExtensions |
| `matrix_inverse.cpp` | Inverse | Strict, NoExtensions |
| `a2d.cpp` | Mixer/filter/conversion | MaxSpeed, Precise, intrinsics, NoExtensions in both configurations |
| `source_reflections.cpp` | Reflection production/history | MaxSpeed, Precise, intrinsics, NoExtensions |
| `a2dbuffer.cpp` | A2D control/HRTF | MaxSpeed, Precise, NoExtensions |
| `source_geometry.cpp` | Geometry traversal | MaxSpeed, Precise, NoExtensions |
| `spatial_audio.cpp` | Spatial producer | MaxSpeed, Strict, intrinsics, NoExtensions |
| `rotation.cpp` | Rotation/edges | MaxSpeed, Fast, intrinsics, NoExtensions, Sync EH |
| generated `geometry_objects.cpp` | OBJECTS | Defaults |
| generated `geometry_finalize.cpp` | FINALIZE | MaxSpeed, Precise, intrinsics, NoExtensions |
| generated `geometry_acoustics.cpp` | ACOUSTICS | MaxSpeed, Strict, NoExtensions |
| generated `geometry_submission.cpp` | SUBMISSION | MaxSpeed, Precise, intrinsics, NoExtensions, Async EH; portable fatal faults terminate via OS |
| generated `geometry_state.cpp` | STATE | MaxSpeed, intrinsics; target FP default |

## Device and acceptance gates

Use real SDL subsystem ownership, open-paused stream, explicit resume and destruction; retain another module's audio ownership. Null device selects the real default. A non-null GUID resolves only an owned, copied, live enumeration mapping; unknown/stale values fail. No pointer/DWORD cast or device-name claim of persistent identity. Device mapping can remain process-local; persistent settings are a separate contract. Exact PCM is measured before SDL conversion, and actual device output is measured separately.

Required software completion evidence: full native module Release/Debug build and ARM64 export/dependency inspection; full factory/ABI/QI/high-address/source/buffer lifetime tests; exact retained mixer receipts and stock-control state parity; explicit EQ/reflection-only invariance; unchanged geometry/reflection producer records; real format/cursor/queue/capacity behavior; the complete typed native notification event/wait/crossing/replacement/close/busy-unload contract; real device output and repeated failure/reinit/shutdown/unload. Sanitizers must exercise production-owned unpadded logical buffers/history, not just padded component inputs.

Keep compilation, module admission, deterministic software parity, actual device playback, OEM semantics and actual consumer routing as separate status columns. Missing OEM or game evidence must not stall independent software implementation, but neither is reported as complete. The full 19-responsibility graph and mandatory-target promotion remain controller-owned aggregate gates. This specification and its preparation report are not execution evidence.
