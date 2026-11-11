# Bounded A3D public-selection/output/coexistence handoff

This explicitly invoked diagnostic fixture is outside default runtime output and broad CTest discovery.
It admits the complete current A3D ABI before resolving the public device factory or invoking any virtual method.
The caller uses independent enumeration declarations and Task 1's separate declaration/descriptor checker.
The profile, record, slot, IID, export and full-descriptor negative admission cases run before factory dispatch.

`--select-device-name NAME` requires exactly one complete, byte-exact UTF-8 snapshot label. It uses the public
`CseCreateA3dNativeDeviceEnumeratorV1` factory, two-call `GetInfo`, caller-owned name/record copies, and the
unchanged `IA3dNativeDeviceV1::InitializeDevice(token, 8, 1)` and `IA3d3::Init(token, 8, 0)` paths. No private
constructor, GUID-field interpretation, persistent token, truncated prefix or default fallback is used.
Index, full copied label, ABI profile, record version and opaque token bytes are diagnostic observations only.
`public_guid_selected_api=PASS` is emitted only after nonnull-token low-level initialization succeeds.
The source path must also succeed before the complete receipt passes.

The host calls `SDL_SetMainReady` on its main thread before enumeration. It retains the enumerator across
both low-level initialization/teardown and source initialization, then releases the anchor once the source
provider holds its own SDL reference. The module lease survives all views and callbacks; final unload and
loader absence are verified for three independent lifetimes. Tokens are ephemeral and same-image/context only.
The harness sets resource mode OFF and exercises public source PCM, Play/Flush, cursor progress, Stop and resume.

In ordinary coexistence mode, the executable compiles exact extracted production DMA records/bodies plus the
unmodified `mpg123_music.cpp` and `mpg123_decoder.cpp`. `prepare.py` verifies DMA field types, offsets, size,
alignment and callable signatures under the actual owning hardware-engine recipe. The probe observes ongoing
DMA position and music state after A3D teardown, then reverses teardown order. This is source-selected DMA/music
coexistence, not a full game launch or an engine A3D consumer. The track is pinned mpg123's `sweep.mp3`.

`--selected-source-only` skips all DMA/music opens. It keeps public token initialization and source playback,
with the instrumented case additionally requiring six successful explicit nondefault opens and zero default
opens across three lifetimes. The observer's full actual device label must equal the public copied label at
both initialization points. Root can therefore select Speakers while the OS default remains the original RODE
output; the fixture performs no route change or default retry. Bare evidence has no stream-open observation.

`a3d_live_bare` has no observer dependency/interpose records; its music evidence is playback state only.
`a3d_live_instrumented` links the transparent test observer. The observer forwards actual SDL open/put calls
unchanged in ordinary cases, admits the A3D and music streams separately on the main control thread, and counts
only successful forwarded PCM submissions. A bounded, preallocated PCM prefix and lock-free counters are the
only producer/callback work; no allocation, locking, logging or file I/O occurs there. Full names and open
metadata are copied on the main control thread. A3D bytes drain after its output closes; music bytes drain/reset
only after `music.Shutdown` joins its producer and destroys its transport. Instrumented coexistence requires
successful music-byte progress within the existing two-second observation bound after A3D teardown, plus
bounded nonzero music PCM. This proves submitted PCM progress, not acoustic consumption or audibility.

The explicit `--inject-put-failure` scenario returns false once per A3D source output lifetime. It does not
inject music failures and is not physical hotplug or Restore. Later cycles create new providers.
Provider path, `dladdr`, function-address and loaded-image checks require one canonical staged SDL provider
shared by the module, executable and observer. Receipts bind the executable, module, SDL, mpg123, track and
(in instrumented mode only) absolute observer-library path and SHA256 before/after, plus source/recipe hashes,
exact argv, ABI profile, selection mode and copied labels.

Configure/build serially in **new** directories, preserving every frozen Task 1/Task 8 graph and receipt:

```sh
# NEW_GRAPH is a separately configured normal runtime graph with the approved providers.
cmake --build "$NEW_GRAPH" --target a3dapi --parallel 1
cmake --build "$NEW_GRAPH" --target cse_stage_mpg123 --parallel 1
cmake -S tests/native/a3d_live -B "$NEW_FIXTURE" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DA3D_GRAPH="$NEW_GRAPH"
cmake --build "$NEW_FIXTURE" --parallel 1
python3 tests/native/a3d_live/run_probe.py --self-test
SDL_AUDIO_DRIVER=dummy "$NEW_FIXTURE/a3d_live_instrumented" --self-test
python3 tests/native/a3d_live/run_probe.py --graph "$NEW_GRAPH" --fixture "$NEW_FIXTURE" \
  --profile Release --evidence "$NEW_EVIDENCE" --instrumented \
  --select-first-enumerated --selected-source-only
```

Use Debug graph/fixture/profile for Debug. `--select-first-enumerated` is dummy-only and still rejects a
repeated friendly label. After observing the dummy label, use exact `--select-device-name` to test name selection.
Omit selection flags to retain default-output compatibility mode; omit `--selected-source-only` for coexistence.
Bare, ordinary instrumented and injected transport cases require separate, newly created evidence directories.
Self-tests perform pure admission/receipt checks and device-less SDL stream success/failure checks, with no
physical device opens. Missing/ambiguous/prefix label matching is tested without manufacturing any token.

The wrapper defaults to asserted process-local SDL dummy. Actual output requires Root's explicit dispatch,
`CSE_ROOT_LIVE_OUTPUT_AUTHORIZED=1`, and exactly one of these modes:

- `--root-authorized-live-selected --select-device-name 'FULL APPROVED LABEL' --selected-source-only`:
  Root verifies the copied Speakers label and retains the original OS output/default/input/system/volume/mute.
  Only the instrumented case supplies explicit nondefault-open and nonzero-PCM observations.
- `--root-authorized-live-selected --select-device-name 'FULL APPROVED LABEL'` (coexistence), or
  `--root-authorized-live-default` (compatibility): Root first uses the separately reviewed temporary Speakers
  default wrapper and restores the original stable UID afterward, leaving input/system/volume/mute unchanged.

Unauthorized, inconsistent or incomplete live options fail before provider/device actions. Unknown/ambiguous
labels fail before any DMA/music/A3D open. The total child-process budget remains 20 seconds with no retry.
No route manipulation, RODE retry, service/security/consent action or microphone open exists in this fixture.

Frozen Task 8 receipts remain default-device evidence; they are **not** rewritten as public-selected-token proof.
Task 1 qualified enumeration under explicit dummy only. Task 2 dummy selected receipts do not establish actual
Speakers selection. Actual Root hardware gates, physical audibility/removal, OEM fidelity, microphone/voice and
genuine game A3D routing remain separate qualifications. The earlier zero-gain low-level DS probe and default
streaming-resource cursor probe remain diagnostic failures, not source/nonzero-PCM acceptance. No production
mixer default, DSP or stream cursor semantic was changed to satisfy this fixture.
