# Native device enumeration V1 — proposed ABI addendum

Status: Root-approved implementation design, not an implementation or a new acceptance claim. Source examined: `/Users/mmalakhouski/.codex/worktrees/native-a3d-source-20261111/CounterStrikeExtended`, baseline `6f27bef51cc50e464d689cf26876fc9adee72156`. Root preparation brief: build/a3d-native-device-enumeration-design-v1/brief.md. This canonical design is normative for scope. Frozen source/build artifacts were read only.

## Intent and boundaries

An independent admitted caller must obtain a genuine module-produced playback-device GUID and pass it to the existing `IA3dNativeDeviceV1::InitializeDevice`. Currently that consumer is public but `A3dNativeOutput::DeviceGuid` is private implementation API. OS-default Speakers selection does not establish this path. No forged GUID, private constructor/layout access, selector interposition, or component-only replacement counts as public selection.

Add an owned native snapshot enumerator and one C factory. Preserve Windows classes, slots, activation, exports at ordinals 2/3, existing native InitializeDevice slot, DSP, mixer/defaults, eight-voice policy, all 21 legacy TUs and generated geometry, all 28 ordered A3D recipes, FP/EH/CRT and route policy. No engine A3D bridge, default mixer change, microphone action, persistent device identity, hardware-capability/default-device claim, physical stream opening merely to enumerate, or generic framework is included.

## Source evidence and approaches

- `GoldSrc-SDK/a3dapi/native_output.cpp:17-69`: `DeviceToken` combines SDL physical ID with `0xa3d0`, token version 1 and a module-local static performance-counter nonce. `DeviceGuid` owns an SDL audio reference until `Close`; `Open` acquires its own reference and compares tokens against current physical playback enumeration.
- `native_provider.cpp:66-94`: provider copies the supplied GUID, validates features/mode, opens/starts output and cleans failure. Initialization mode stays 1, direct feature stays `0x08`; null GUID still selects default.
- `native_device_init.h`: native V1 has exactly the IUnknown prefix and InitializeDevice. Do not append slots.
- `GoldSrc-SDK/public/cse_a3d_abi.h:552-591`: descriptor uses all 16 entries. Native device/capacity are already bundled under PrivateDeviceContract. Records and C exports are bundled independently.
- `cse_module_abi.h`: descriptor version 1, runtime version 10, capacity 16, size 288, `Check` requires exact profile matches. No generic descriptor change is necessary.
- `native_notifications.cpp:391-431`: independent owned C event factory is an established native export pattern. `A3d3.cpp:53-69` checks native owners before returning S_OK for unload.
- Pinned SDL source below `build/a3d-native-20261111/graph-task7-debug/dependencies/SDL3/source/SDL3-3.4.20`: `src/audio/SDL_audio.c:1499-1534` allocates and returns a physical ID snapshot under the subsystem rwlock; null is failure and a nonnull zero-count array is valid. `:1592-1640` copies device names under that lock through `SDL_GetPersistentString`. `src/SDL_utils.c:420-470` places those copies in per-thread TLS storage: immediately copy into our owned snapshot on the same thread; do not expose the SDL pointer. Device removal can invalidate an ID between these calls. `:357-367,956-963` uses monotonic instance IDs; do not turn that implementation into a persistent identity promise.
- Pinned `include/SDL3/SDL_init.h:249,267`: initialization is main-thread-only and quit is not thread-safe. A refcount alone does not make lifetime transitions free-threaded. Enumeration/name queries themselves are documented any-thread safe.

Chosen: an independent factory returns an immutable, owned snapshot. It supports selection before provider construction, makes the enumeration lifetime explicit, and avoids holding provider/mixer state merely to populate a list. Factory lifetime ends when the caller releases it; no global permanent SDL reference is introduced.

Alternative: a distinct new QI view on the native provider can preserve old vtables, but requires provider activation before selection, mixes enumeration and playback lifetimes, and expands provider QI/identity paths. It offers no advantage for this small producer-consumer gap. Extending existing V1 is rejected because it mutates the promised vtable; C malloc strings or raw SDL IDs would add ownership/representation leaks.

## Exact public declarations

New header: `GoldSrc-SDK/public/cse_a3d_devices.h`, with native-only availability and the existing platform COM/module ABI headers. The following names, types, parameter order and values are normative:

```cpp
constexpr DWORD A3D_NATIVE_DEVICE_ENUMERATION_VERSION = 1;
constexpr IID IID_IA3dNativeDeviceEnumeratorV1 = {
    0x7dcfb8c4, 0xf874, 0x5872, {0x9c, 0x0f, 0xd4, 0x17, 0xca, 0x72, 0xdd, 0x2a}
};
struct A3dNativeDeviceInfoV1 {
    DWORD structureBytes;
    DWORD version;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
struct IA3dNativeDeviceEnumeratorV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD index, DWORD recordVersion, DWORD recordBytes,
        A3dNativeDeviceInfoV1* info, char* name, DWORD nameCapacity, DWORD* requiredNameBytes) = 0;
};
extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeDeviceEnumeratorV1(
    IA3dNativeDeviceEnumeratorV1** enumerator);
```

IID derivation: RFC UUIDv5, URL namespace `6ba7b811-9dad-11d1-80b4-00c04fd430c8`, exact case-sensitive UTF-8 name `CounterStrikeExtended/IA3dNativeDeviceEnumeratorV1`. Result `7dcfb8c4-f874-5872-9c0f-d417ca72dd2a`, verified with Python `uuid.uuid5(uuid.NAMESPACE_URL, name)` during design.

No virtual destructor. Ordered slots: 0 `HRESULT QueryInterface(REFIID, void**)`; 1 `ULONG AddRef()`; 2 `ULONG Release()`; 3 `HRESULT GetCount(DWORD*)`; 4 `HRESULT GetInfo(DWORD, DWORD, DWORD, A3dNativeDeviceInfoV1*, char*, DWORD, DWORD*)`. All methods use STDMETHODCALLTYPE as the current native COM declarations do. C export is unmangled and uses CSE_ABI_CALL. HRESULT is signed 32-bit; ULONG/DWORD are unsigned 32-bit; char is one byte; pointer width/alignment comes from admitted native platform. No C++ size_t/bool/std::string crosses this interface.

Record size 32, alignment 4, standard layout; static assertions are mandatory:

| Field | Type/width | Offset | Alignment |
|---|---|---:|---:|
| structureBytes | DWORD / 4 | 0 | 4 |
| version | DWORD / 4 | 4 | 4 |
| device | GUID / 16 | 8 | 4 |
| nameBytes | DWORD / 4 | 24 | 4 |
| reserved | DWORD / 4 | 28 | 4 |

GUID retains its actual fields: Data1 u32 at 0; Data2 u16 at 4; Data3 u16 at 6; Data4 byte[8] at 8; size16/align4. Success writes `structureBytes=32`, `version=1`, `reserved=0`, token and `nameBytes` including trailing NUL. Records and names contain no module-owned pointers.

## Results, bounds and copied names

Use existing signed HRESULT bit patterns: S_OK `0x00000000`, S_FALSE `0x00000001`, E_NOINTERFACE `0x80004002`, E_POINTER `0x80004003`, E_FAIL `0x80004005`, E_OUTOFMEMORY `0x8007000e`, E_INVALIDARG `0x80070057`.

Factory: if enumerator is null, E_POINTER. Otherwise clear `*enumerator` before any fallible work. On success return S_OK and reference count 1. Failed SDL initialization, null enumeration, negative count, unavailable name during snapshot, or unsupported length return E_FAIL; local allocation failure returns E_OUTOFMEMORY. No partial view escapes. The host must first call SDL_SetMainReady on its actual main thread (or already have established SDL main-thread identity there). With that precondition, factory called off SDL main thread returns E_FAIL before initializing SDL. Do not infer allocation failure from SDL error text: SDL query failure is E_FAIL.

`GetCount`: null pointer returns E_POINTER; otherwise writes actual immutable snapshot count and S_OK, including zero. There is no invented minimum device, hardware capacity, or default placeholder.

`GetInfo` validation/cleanup is ordered:

1. If requiredNameBytes is nonnull, set it to zero. If info is nonnull, clear exactly `min(recordBytes, DWORD(32))` bytes. If name is nonnull and nameCapacity is positive, clear name[0]. All pointers must designate writable, nonoverlapping storage for the stated sizes; invalid addresses/lies about allocation are outside an in-process ABI's contract.
2. Null info or requiredNameBytes, or null name with nonzero nameCapacity: E_POINTER. Null name with zero capacity is an allowed query. No record input fields are read.
3. recordVersion != 1, recordBytes != 32, or index >= snapshot count: E_INVALIDARG. The cleared outputs remain; no write outside the explicitly supplied record span. Larger supplied spans are rejected too and bytes beyond 32 remain unchanged.
4. Set requiredNameBytes to the snapshot label's full UTF-8 byte count including NUL. If nameCapacity is smaller, return S_FALSE; info stays zero and no partial string/token is returned. S_FALSE here means a successful size query/insufficient caller capacity, not failed enumeration. Even an empty label needs one byte.
5. Copy the complete label plus NUL and complete info, return S_OK. Oversized buffers are allowed; bytes beyond the copied name and record stay unchanged. No truncation flags are necessary because no truncation occurs. Caller may allocate its own buffer between calls; immutable snapshot guarantees the requirement does not change.

Labels are friendly text only, never identifiers. Copy the exact SDL UTF-8 label. Validate UTF-8 at snapshot construction; an invalid sequence returns E_FAIL rather than publishing malformed text or silently renaming a device. Empty valid labels are allowed. Length must fit DWORD including NUL; overflow fails E_FAIL. Use checked allocation arithmetic and nothrow/explicit allocation because native recipes include no-exception paths. Allocation ownership remains entirely within the module; only caller-owned copied bytes cross the ABI.

`QueryInterface`: only IID_IUnknown and the new IID succeed; return the same canonical unknown identity, AddRef once, clear valid output on every failure, E_POINTER for null output and E_NOINTERFACE for unsupported IID. It is not a provider view. AddRef/Release return actual remaining reference count; terminal Release destroys the exact concrete type without a virtual destructor slot.

## Snapshot construction and lifetime

Implement the concrete enumerator and C factory in existing `native_output.cpp`; no new production TU is needed. Share that TU's existing `DeviceToken(SDL_AudioDeviceID)` function unchanged with DeviceGuid/Open. Obtain one ID array and derive each record's token and copied name from that same ID. Do not enumerate twice and feed the old ordinal into DeviceGuid: list reordering/hotplug would pair the wrong name and token. This is the same-image production token producer, not a duplicate formula in caller code.

Factory holds a transient `A3dNativeOwnerLease(A3dNativeOwnerKind::Output)` across acquisition/unwind. Each returned object holds its own Output lease from construction through destruction and one successful `SDL_InitSubSystem(SDL_INIT_AUDIO)` matched with exactly one `SDL_QuitSubSystem(SDL_INIT_AUDIO)`. Reusing Output means an output-subsystem lifetime owner; it does not claim a stream was opened and avoids changing the existing 14-entry private owner snapshot. Failed allocation/name/enumeration destroys all copied entries, frees the SDL array, drops audio ownership and owner leases. No open/resume/producer-thread function is used for enumeration.

Construction/final Release and provider initialization/teardown must run on the host SDL main/control thread and be serialized with every other host SDL init/quit transition. Factory verifies SDL_IsMainThread. Pinned src/SDL.c:272-284 returns true when SDL has not recorded any main/events/video thread, so this check alone cannot enforce cold-start main-thread identity; the host SDL_SetMainReady precondition is mandatory. GetCount/GetInfo can concurrently read the published immutable snapshot; QI/AddRef/Release use the existing atomic reference helpers. Each active caller holds its own reference. The host keeps an anchor reference on the main thread, joins/drains concurrent users, then makes terminal Release there. No final Release from an audio callback, no async destructor, no unbounded wait, and no claim that a module mutex can serialize unrelated host SDL calls. Refcount operations cannot diagnose a wrong-thread terminal Release via HRESULT; this is an explicit caller precondition, covered by documentation and positive lifecycle tests.

The host's dlopen/library lease covers all factory/virtual calls, concurrent readers, and final Release. An enumerator alone makes DllCanUnloadNow return S_FALSE; terminal Release with all other owners gone makes S_OK. No object can itself prevent a hostile caller from ignoring DllCanUnloadNow and dlclosing live code; this matches existing host-lease policy.

Caller sequence: load and admit module; factory; GetCount; query-size GetInfo then full GetInfo; activate existing provider; QI existing IA3dNativeDeviceV1; call InitializeDevice with copied token, features `0x08`, mode `1`, supported output. Keep enumerator until InitializeDevice returns. On success provider owns its own SDL reference before enumerator release; on failure keep/release enumeration as desired and clean provider. Keep host library lease until every view is released. Select by current snapshot index/name only for the current interaction; do not persist GUID, ordinal or name as identity.

A lease prevents accidental subsystem shutdown by this participant, not hotplug. A removed device may remain in the copied list; InitializeDevice revalidates and may return E_INVALIDARG (no current token) or E_FAIL (open/start race). Supported features remains zero on failure under the existing implementation. GetInfo reports the snapshot, not live availability. Refresh means release/create another enumerator; no Refresh method or event pump is required. The factory may fail if a device disappears during name copying; do not silently skip failed items or substitute a label. A caller may explicitly retry later; no automatic indefinite retry is introduced.

Tokens are opaque, ephemeral and nonpersistent. Existing performance-counter nonce is not cryptographic or a proof of globally unique module instances. Normatively supported tokens come from the same still-loaded image and still-live audio context. Random/corrupt/no-longer-enumerated tokens are rejected by actual comparison; a foreign token with different nonce/ID is rejected. Universal foreign-token collision prevention and stale-token authenticity are not promised. Tests must obtain foreign/reloaded tokens from actual factories, observe actual differences, and never manufacture a selectable token. Reload invalidates the supported-use contract even if implementation bits happened to coincide. Keeping only a copied record does not retain SDL or module lifetime.

## Admission profile

Keep generic DescriptorVersion=1, RuntimeVersion=10, MaxTables=16, descriptor size288, table identifiers/order/count unchanged. Add `constexpr std::uint32_t NativeDeviceEnumerationProfile = 1` in cse_a3d_abi.h and append it to ProviderRole's existing hash facts. Keep shape-schema Revision=1: it describes normalized signature encoding, not a global feature count. Existing contracts' semantics are preserved; this additive native profile is represented by owning-family hashes rather than a global runtime bump/rebuild of unrelated modules.

Append the new record size/alignment and every field offset/width/alignment/type to Records. Add the exact five-slot ordered enumerator signature and no-destructor assertion; append its slots, new IID and enumeration version to PrivateDeviceContract. Append factory signature to ExportContract, after the existing signatures; include an explicit export-name hash with schema tag1, the byte count excluding NUL, then each unsigned ASCII byte of CseCreateA3dNativeDeviceEnumeratorV1 in order (name included because signature alone cannot distinguish identical-shaped exports). Append new IID to IdentityContract. Hash the enum version, successful recordBytes 32 and S_FALSE query-size policy tag `1` under PrivateDeviceContract. These semantic constants distinguish this contract without changing generic ABI machinery.

The caller obtains only CseGetModuleAbi before admission. It validates the entire expected A3D descriptor using Check, then resolves required C exports and only then invokes factory/virtual calls. Missing factory after descriptor match is a hard admission failure. Old A3D callers reject the new profile and new callers reject old modules via TableLayout; rebuild/relink the A3D native test/live consumers together. This is additive source/public functionality with deliberately exact-profile binary admission, not a claim of drop-in old native binary compatibility. Windows exports and native V1 slot order remain unchanged.

Independent caller declarations must reproduce the new IID/record/slots/export typedef without including cse_a3d_devices.h in the translation unit that exercises them. A separate checker compares production declarations/shape facts against the independent caller; shared-descriptor inclusion alone cannot prove declaration independence. Negative tests mutate record field offsets/widths/alignment/order, method order/parameter widths, IID, export name/signature/profile and each descriptor entry before calls; count zero factory/virtual calls for every rejected admission. Do not extend generic descriptor capacity to 17.

## Acceptance boundaries

Actual loaded production dylib under SDL dummy must prove public factory → copied record/token → unchanged InitializeDevice, copied name survival, real zero count when the backend genuinely has none, all failure cleanup, held-enumerator unload refusal, main-thread final cleanup, concurrent immutable reads, and repeated unload/reload. Internal deterministic failure tests can exercise allocation/SDL null/empty/name-failure branches at the existing operations seam, but do not substitute for actual factory tests. No new public test-only hook/factory is allowed. A backend's dummy-device count is observed, not hard-coded as hardware capacity.

Root's separate live gate must enumerate the approved Speakers label, require an unambiguous complete copied match, and initialize with that returned GUID. It must log public enumeration/selected-token evidence without GUID fabrication and verify actual stream/provider identity using the existing transparent observer. Keep bare/instrumented/injected receipts separate. Strengthen music evidence to real successful PCM delivery after A3D teardown, not IsPlaying alone; include capture-library SHA in instrumented receipts. Nonzero submitted PCM and progress do not prove audible sound, OEM fidelity, hardware unplug/Restore, microphone operation or genuine game A3D integration.


## Root execution addendum after reviewed Task1

Task1 implementation9fb1450/test-fix480be07 is reviewed/integrated and software-qualified with explicitdummy only. Native exactprofile and existing Windows/oldnativeV1 contracts are preserved. All34007priorartifacts are frozen.

Task2 must use NEW graph/fixture/evidence roots under build/a3d-native-selected-probe-20261111; never build cse_stage_mpg123 or any target in the frozen Task1 graphs. Reuse only the immutable byte-identical existing provider mirror build/a3d-native-enumeration-20261111/prefix-mirror-001 as read-only provider input, with explicit typed CSE_NATIVE_THIRD_PARTY_PREFIX:PATH. Same original CEF/Steam provider values, allserial1/global2. Build new a3dapi plus cse_stage_mpg123 and standalone live fixtures, not full19.

Root-selected hardware proof must distinguish explicit selection from OS default. Add harness-only --selected-source-only mode: no engine DMA/music owner opens, one real public factory/copied token→unchanged provider/source path with nonzeroPCM/cursor proof, no GUID private-field decode. For Root's actual selected-only gate the OSdefault stays originalRODEUID while A3D selects the unambiguous full copied Mac Studio Speakers label; this does not open/retryRODE. Read-only Root metadata proves default/input/system/volume/mute unchanged. Instrumented observer must record the actual forwarded explicit nondefault SDL open request and full actual selected label; compare with public copied label without redirecting any call. Bare mode states its narrower observation. Unknown/truncated/ambiguous label or default request fails the selected gate.

Full DMA/music coexistence remains a SEPARATE test under Root's approved temporarySpeakersdefault/restore wrapper. Strengthen successful musicPCM submission progress after A3D teardown, bind observerlibrary hash in instrumented receipts, and keep bare/instrumented/injected/selected-only results separate. Neither queue/cursor/nonzeroPCM nor selected metadata proves physical audibility, unplug/Restore, microphone/OEM/game routing. Sourceworker onlydummy; Rootalonephysicalcases afterreview. No defaultRODEOpen retry/service/privacy changes.
