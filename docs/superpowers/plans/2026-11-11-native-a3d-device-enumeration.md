# Native Device Enumeration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task under Root's existing execution ownership. This design-preparation task creates no subagents. Root may assign later execution/review under its own instructions. Steps use checkbox syntax for tracking.

**Goal:** Let an independently admitted public native caller enumerate copied playback-device tokens and initialize the unchanged provider with an actual selected token.

**Architecture:** A new owned IUnknown snapshot is created by CseCreateA3dNativeDeviceEnumeratorV1. Its native implementation shares the existing module-local DeviceToken producer in native_output.cpp and retains SDL/output ownership until Release. Existing descriptor families absorb the new ABI facts without a generic ABI expansion.

**Tech Stack:** C++17, existing native COM shims, pinned SDL 3.4.20, Darwin arm64 ABI, CMake/Ninja, Python contract checks, actual production a3dapi.dylib.

**Spec:** [canonical design](../specs/2026-11-11-native-a3d-device-enumeration-design.md). The canonical design binds this plan; Root owns execution and review. Paths below are relative to the existing source worktree `/Users/mmalakhouski/.codex/worktrees/native-a3d-source-20261111/CounterStrikeExtended` unless marked Root.

## Global Constraints

- Baseline `6f27bef51cc50e464d689cf26876fc9adee72156`; source5275/new10451old14189artifactsfrozen. No old graph or frozen artifact is rebuilt/overwritten.
- SourceworkerMac1/global2/allserial1. Existing worktree only; no new dependencies/worktree. Root authorizes the later source handoff after Task8 review/integration.
- Preserve all28 prior A3D ordered recipes/all21 legacy TUs/generated geometry, Windows routes, native flags, FP/EH/CRT, all old slots and Windows exports2/3.
- New IID `7dcfb8c4-f874-5872-9c0f-d417ca72dd2a`; enumeration version1; record size32/align4; five slots; main-thread audio lifetime transitions; exact query-size copy behavior in spec.
- Generic DescriptorVersion1/RuntimeVersion10/MaxTables16 stay unchanged. New native A3D enumeration profile1 invalidates old exact-profile callers.
- Rootonlyphysicaldevicecode; defaultRODEindependentHALstalluntouched/noService/sudo/privacy. Micconsentuserexplicitdefer. No physical/audibility/OEM/gameconsumerclaimfromdummy.
- Native enumerator has no stream open, default-device query/open, provider construction or permanent global SDL lease. Selection remains the caller's explicit copied token.
- No user questions. These are proposed future commands, not commands run by this design task.

## Review Focus

1. Snapshot list reorder/removal between enumeration and name lookup: fail/retain snapshot as specified, never pair a new ordinal with the old name (Task1 tests).
2. Tiny, zero, oversized, invalid-version buffers and UTF-8 labels: bounded clearing, complete-copy-only S_FALSE semantics, no overflow or borrowed pointers (Task1 tests).
3. Host coexistence/final Release: main-thread audio ownership and joined readers, held snapshot prevents unload, other SDL owner survives (Task1 tests).
4. Old/wrong ABI or missing export: no factory/virtual call before full admission (Task1 tests).
5. Physical selection and continuing music evidence: real token on approved route, actual music successful delivery after A3D release, fully hashed observer, no claim inflation (Tasks2–3).

---

## Task 1: Public native enumeration, owned snapshot, actual module qualification

**Files:**
- Create `GoldSrc-SDK/public/cse_a3d_devices.h` (exact public ABI).
- Modify `GoldSrc-SDK/public/cse_a3d_abi.h` (profile/records/slots/export/name/IID hashes).
- Modify `GoldSrc-SDK/a3dapi/native_output.cpp` (factory and concrete snapshot beside existing shared DeviceToken; no new production TU).
- Modify `GoldSrc-SDK/a3dapi/native_exports.cpp` (new C export type static_assert).
- Modify `tests/native/a3d_module/CMakeLists.txt`, `check_contract.py`, `README.md`.
- Create `tests/native/a3d_module/enumeration_caller.h`, `enumeration_tests.cpp`, `enumeration_admission.cpp`, `enumeration_faults.cpp`.
- Update `tests/native/a3d_module/module_tests.cpp` only for newly required export/profile admission, retaining existing tests.
- Update `tests/native/a3d_live/README.md` to point to the delivered API while keeping the live selected gate unrun until Task3.

**Interfaces:** Consumes existing DeviceToken(SDL_AudioDeviceID), native reference helpers/Output owner lease, SDL APIs and existing InitializeDevice. Produces exactly the public header block, result/ownership contract and hash profile in 2026-11-11-native-a3d-device-enumeration-design.md. `enumeration_caller.h` contains independently declared caller types in namespace `independent`; no production device header is included by enumeration_tests.cpp. `enumeration_admission.cpp` supplies `cse::abi::Descriptor ExpectedA3dEnumerationDescriptor()` and compile-time comparison of independent record/method facts against production facts, using independent declarations rather than type aliases. The caller uses only independent vtable declarations across the dylib boundary.

- [ ] Add contract tests first: UUIDv5 derivation; exact record field offsets/widths/alignments/size; five ordered slots/no destructor; all argument widths/order; factory C signature and literal export name; old IA3dNativeDeviceV1 slot list unchanged; descriptor count16; Windows export file unchanged. Run `python3 tests/native/a3d_module/check_contract.py "$PWD"`; initial failure must identify absent enumeration declarations, not unrelated source errors.
- [ ] Add actual module cases in new `a3d_enumeration_tests` target: `public_selected_initialize`, `copied_record_name`, `sized_output_validation`, `empty_snapshot`, `held_enumerator_unload`, `shared_audio_ownership`, `concurrent_snapshot_reads`, `snapshot_failure_cleanup`, `stale_foreign_tokens`, `enumeration_admission_mutations`. Register each with SDL_AUDIO_DRIVER=dummy and TIMEOUT30. A real successful factory must be the only positive producer of accepted GUIDs.
- [ ] Pin assertions: factory null output E_POINTER; valid output cleared on factory failure; QI canonical unknown/refcount balance; unsupported IID clears output; GetCount null E_POINTER; count equals successful record range; no hardcoded count. For each record GetInfo query returns S_FALSE, required >=1, info zero; exact and larger name buffers return S_OK and matching names/token; invalid index/version/bytes return E_INVALIDARG with bounded clearing and canaries intact. Test recordBytes 0,1,31,32,33 and name capacities 0,required-1,required,required+1; null pointer combinations; repeated queries; caller copy survives enumerator destruction without reusing token after lifetime expiry. Long/multibyte/empty/malformed names need deterministic labeled fault scenarios.
- [ ] Add narrow test-only fault observer in enumeration_faults.cpp, loaded only by explicit dummy subprocess cases. It interposes the real SDL init/enumeration/name/allocation calls to return one requested error/empty snapshot or label fixture; ordinary calls forward unchanged. It does not redirect selected IDs, intercept provider selection, introduce product exports, replace the factory, or count as real-device evidence. Limit allocation failure to the observed acquisition interval. Production snapshot allocation uses checked SDL_malloc/SDL_free so fault cleanup can be exercised without exception policy changes. Register child fault scenarios through a3d_enumeration_tests; isolate each process and verify all owner counts/unload return to baseline. A synthetic empty list proves empty-branch semantics only; a real nonnull zero-count list is accepted whenever encountered.
- [ ] Run these tests against a freshly copied baseline actual dylib in a new fixture directory; require failure at missing new export/profile before any new factory/virtual call. Preserve the red receipt; do not reuse/alter the baseline binary.
- [ ] Implement the exact header and native-output factory/snapshot. Use one ID snapshot, immediate same-thread owned name copy, unchanged DeviceToken(id), no repeated DeviceGuid(index) mapping. Hold transient and returned Output owner leases, main-thread SDL ownership, checked nothrow/SDL allocation and complete failure unwind. Host tests call SDL_SetMainReady on actual main before creating worker callers; factory off-main then returns E_FAIL with null output. Document that unregistered SDL_IsMainThread can return true and does not identify a cold host main thread. Keep counts/records immutable and refcount atomic. No new TU/manifest entry is needed.
- [ ] Implement admission hashing exactly as spec: record facts, slot list, IID, C factory signature/name, semantic profile/query policy. Use a constexpr name-facts helper with LayoutHash facts schema tag1, ASCII byte count excluding NUL, then each unsigned ASCII byte in order. Static_assert the production C factory pointer against the declared typedef in native_exports.cpp. Do not touch generic module ABI version/capacity.
- [ ] Finish negative admission tests: missing descriptor/export, mutated common fields, each table ID/signature/reserved field, new record shape, new method order/parameter width, new IID, export signature/name, query-size policy/profile and old baseline. Count factory and virtual calls; every rejection must leave both zero. Test new caller rejects old module and old expected descriptor rejects new module. Independently generate changed expected facts rather than only toggling opaque output hashes.
- [ ] Build only fresh graphs, serially, with the exact baseline provider options below. First build a3dapi, then new/old module tests and the focused output/resource suites in both profiles. After review, full19 rebuilding is a Root decision outside this task; never rebuild frozen full19 graphs merely to add a small API.
- [ ] Actual public selection test: factory → count/info size query/copy → existing DllGetClassObject/CreateInstance/QI → InitializeDevice(&copied.device,0x08,1,&supported). Retain enumerator until call completes, verify supported0x08 and real existing source playback path, release enumerator and confirm provider remains usable, then final release/unload. Null/default behavior remains tested independently.
- [ ] Prove an enumerator alone returns S_FALSE for DllCanUnloadNow; final main-thread release returns S_OK and dlclose followed by RTLD_NOLOAD proves absence. Run multiple enumerators and main-held-anchor/concurrent readers; join before terminal Release. Check other real SDL owner survives enumeration release. Foreign/reloaded tokens come from actual module factories; document observed different contexts and test existing E_INVALIDARG when comparison cannot match. Do not claim cryptographic universal rejection.
- [ ] Compare all28 ordered recipes to frozen baseline after only graph-directory normalization; all21 legacy TUs/generated geometry and Windows export hashes remain unchanged. If implementation unexpectedly needs a new TU, stop and return a concrete revised design to Root; never hand-edit generated target definitions.
- [ ] Commit scoped source/test changes and send Root the exact commit, green/red receipts, new artifact paths, unresolved limitations and source/slot/policy diff for independent review. No live-device execution in this task.

### Exact build/test command template

Use a new output root `build/a3d-native-enumeration-20261111`; fail rather than overwrite if its intended output directory already exists. Execute each profile serially. Use profile=release then debug, config=Release then Debug; each shell variable is task-specific. No commands below were executed by design preparation.

```sh
export CMAKE_BUILD_PARALLEL_LEVEL=1
export CTEST_PARALLEL_LEVEL=1
cse_enum_profile=release
cse_enum_config=Release
cse_enum_graph="$PWD/build/a3d-native-enumeration-20261111/graph-$cse_enum_profile"
cmake --preset "runtime-macos-arm64-$cse_enum_profile" -B "$cse_enum_graph" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  '-DCEF_ROOT=/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/providers/cef/extracted-macosarm64/cef_binary_156.0.26+g8ea0c73+chromium-156.0.8078.88_macosarm64' \
  '-DCSE_NATIVE_THIRD_PARTY_PREFIX=/Users/mmalakhouski/Documents/GitHub/CounterStrikeExtended/build/native-third-party/macos14/cse-arm64-macos' \
  '-DCSE_STEAM_PROVIDER_FILE=/Users/mmalakhouski/Library/Application Support/Steam/steamapps/common/Half-Life/libsteam_api.dylib'
cmake --build "$cse_enum_graph" --target a3dapi --parallel 1
cmake -S tests/native/a3d_module -B "build/a3d-native-enumeration-20261111/module-$cse_enum_profile" \
  -G Ninja -DCMAKE_BUILD_TYPE="$cse_enum_config" -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DA3D_MODULE="$cse_enum_graph/runtime/$cse_enum_config/a3dapi.dylib"
cmake --build "build/a3d-native-enumeration-20261111/module-$cse_enum_profile" --parallel 1
ctest --test-dir "build/a3d-native-enumeration-20261111/module-$cse_enum_profile" --output-on-failure --parallel 1
```

Foreign-context test owns a byte-identical copy of the new dylib under a distinct new test path, with the same canonical staged SDL provider. Record whether loader actually loaded a distinct image; skip no collision-sensitive premise silently. Test harness supplies that path using existing A3D_FOREIGN_MODULE configuration when executing the foreign case; never treat an alias of the same image as foreign.

## Task 2: Selected public-token probe and stronger coexistence receipts

**Files:** Modify `tests/native/a3d_live/live_probe.cpp`, `capture.cpp`, `capture.h`, `run_probe.py`, `README.md`; CMakeLists.txt only if independent caller admission needs the separate Task1 helper TU.

**Interfaces:** Consume Task1 factory/ABI and existing provider. Add wrapper option `--select-device-name NAME` (exact full UTF-8 copied label; require exactly one match; never choose a truncated prefix/default silently). Carry existing `--root-authorized-live-default` only as compatibility/default-mode selection; add distinct `--root-authorized-live-selected` requiring `--select-device-name` and CSE_ROOT_LIVE_OUTPUT_AUTHORIZED=1. Dummy may use `--select-device-name` with observed dummy label or a dedicated `--select-first-enumerated` option limited to dummy; live cannot use the latter. Report `public_guid_selected_api=PASS` only after successful nonnull-token InitializeDevice from admitted enumerator.

- [ ] Add red harness validation for missing/ambiguous selected name, new profile rejection, wrong live option combinations, and instrumented receipt missing capture SHA. Preserve existing default cases and failure-mode separation.
- [ ] In Module keep enumeration ownership through InitializeDevice. Read actual names using two-call GetInfo; log index, full copied label, factory/record version and selected token bytes as opaque diagnostics. No test/private constructor/dlsym mangled token producer is allowed. Feed selected GUID into both low-level provider InitializeDevice and existing source path api->Init(&copied.device, kDirectFeature, 0), whose A3dScene.cpp helper forwards to unchanged InitializeDevice with backend mode1; do not let the subsequent source Api() silently switch back to null/default.
- [ ] Extend transparent observer with a second explicitly admitted music stream, set only around music.Open on main control thread. Existing music uses SDL_OpenAudioDeviceStream; count successful real SDL_PutAudioStreamData returns for that stream and bounded nonzero PCM without changing data/return values. Snapshot successful-byte counters before/after OwnersProgress after A3D teardown and require increase within existing bounded progress timeout. Capture callback/producer paths stay preallocated, lock-free, nonblocking and free of logging/I/O; no production telemetry or callback replacement. Track the stream until music.Shutdown joins its producer and destroys transport before resetting/draining captured memory. Bare case must report DMA progress + music state only; only instrumented case claims observed music PCM submission progress. Successful submission is not acoustic consumption/audibility.
- [ ] Include absolute `liba3d_live_capture.dylib` path and SHA256 in files_before/files_after only when instrumented. Record selection mode, copied selected label, ABI profile, source/image/provider hashes and exact argv. Verify observer and executable use the same single canonical SDL provider as the actual module. Keep ordinary transparent, bare, injected-transport and real-output receipts in separate new directories.
- [ ] Stage the already pinned music dependency in the new graph with `cmake --build "$cse_enum_graph" --target cse_stage_mpg123 --parallel 1`; this target is defined in cmake/Mpg123Dependency.cmake and requires no full19 rebuild. Configure fresh fixtures against the new Task1 graph with the existing README command (`-S tests/native/a3d_live`, `-DA3D_GRAPH` set to new graph); build --parallel1. Run dummy default/selected bare, instrumented and injected cases in Release and Debug using the wrapper, each in a new evidence directory. Verify source playback/cursor/nonzero PCM plus continuing successful music delivery after A3D teardown, reverse teardown, no leaked SDL/module owners, and capture SHA binding.
- [ ] Commit harness changes separately and send exact Root-only live command with new immutable graph/fixture/evidence paths after review. Preserve Task8 claims about frozen default-device evidence; do not rewrite them as selected-token proof.

## Task 3: Root-only actual Speakers selected-token gate

**Files:** New Root evidence receipts/handoff and canonical acceptance addendum only; no source changes unless review reveals an issue.

- [ ] Root completes source/recipe/module/harness review and obtains exclusive physical-device ownership. Use the previously reviewed output-only Speakers selection/restoration wrapper with new artifact paths; do not change its routing/security policy or repeat unchanged RODE. Input, system-output, volume and mute remain protected by the wrapper's existing checks.
- [ ] Run one bounded selected case with `--root-authorized-live-selected --select-device-name 'Mac Studio Speakers'`, exact public enum name checked first by the harness, and CSE_ROOT_LIVE_OUTPUT_AUTHORIZED=1, from the approved wrapper. An absent/ambiguous/nonmatching label is a failed selection gate, not permission to fall back or invent a token. The wrapper may set the approved default for independent DMA/music coexistence; A3D selection itself must pass a nonnull returned GUID.
- [ ] Verify actual selected stream label/provider, token provenance, source PCM/cursor and owner/coexistence receipts. Run bare/instrumented/injected variants only as independently authorized in Root's physical budget, never combine their claims. Bound process at20s, no automatic retry/device intervention. Include observer hash in the instrumented receipt.
- [ ] Restore/verify original stable output UID through the existing wrapper and record result. Report remaining physical audibility, unplug/Restore, unchanged RODE, microphone/OEM and game-consumer limitations explicitly. Only now mark the public selected-device implementation/live subgate according to actual evidence; do not mark overall Apple Silicon migration complete.

## Self-review coverage

ABI/error/copy/UUID/admission → Task1. Producer-consumer ownership and hotplug → Task1 plus live Task3. Old-slot/Windows/recipe preservation → Task1. Music progress/capture hash carried minors → Task2. Root physical restrictions and exact selected-vs-default evidence → Task3. No source/API question is deferred to the user; failed implementation premises return to Root with evidence. Physical route availability and nonce-collision-strength limitations remain explicit risks, not invented software guarantees.

Root ruling: the live name argument is the full current observed label Mac Studio Speakers, not an abbreviation. Root must confirm actual SDL enumeration produces that exact unambiguous copied label before the physical selection attempt; absence/drift fails without fallback. Task1 uses dummy only. Canonical docs commit adds two files to the5275-file code baseline; frozen6f27 evidence remains immutable.


## Root execution addendum after reviewed Task1

Task1 implementation9fb1450/test-fix480be07 is reviewed/integrated and software-qualified with explicitdummy only. Native exactprofile and existing Windows/oldnativeV1 contracts are preserved. All34007priorartifacts are frozen.

Task2 must use NEW graph/fixture/evidence roots under build/a3d-native-selected-probe-20261111; never build cse_stage_mpg123 or any target in the frozen Task1 graphs. Reuse only the immutable byte-identical existing provider mirror build/a3d-native-enumeration-20261111/prefix-mirror-001 as read-only provider input, with explicit typed CSE_NATIVE_THIRD_PARTY_PREFIX:PATH. Same original CEF/Steam provider values, allserial1/global2. Build new a3dapi plus cse_stage_mpg123 and standalone live fixtures, not full19.

Root-selected hardware proof must distinguish explicit selection from OS default. Add harness-only --selected-source-only mode: no engine DMA/music owner opens, one real public factory/copied token→unchanged provider/source path with nonzeroPCM/cursor proof, no GUID private-field decode. For Root's actual selected-only gate the OSdefault stays originalRODEUID while A3D selects the unambiguous full copied Mac Studio Speakers label; this does not open/retryRODE. Read-only Root metadata proves default/input/system/volume/mute unchanged. Instrumented observer must record the actual forwarded explicit nondefault SDL open request and full actual selected label; compare with public copied label without redirecting any call. Bare mode states its narrower observation. Unknown/truncated/ambiguous label or default request fails the selected gate.

Full DMA/music coexistence remains a SEPARATE test under Root's approved temporarySpeakersdefault/restore wrapper. Strengthen successful musicPCM submission progress after A3D teardown, bind observerlibrary hash in instrumented receipts, and keep bare/instrumented/injected/selected-only results separate. Neither queue/cursor/nonzeroPCM nor selected metadata proves physical audibility, unplug/Restore, microphone/OEM/game routing. Sourceworker onlydummy; Rootalonephysicalcases afterreview. No defaultRODEOpen retry/service/privacy changes.
