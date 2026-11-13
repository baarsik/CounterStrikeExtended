# Native A3D resources and ownership

This fixture compiles the actual retained `a3dapi` sources, all five geometry
wrappers, and the native owners from the generated source manifest. Per-file
FP/EH/RTTI/optimization options are evaluated from the retained MSBuild manifest.
It uses the real provider, distinct leaf, resource manager, stream buffers,
`DalBufferInfo`, native mixer/output, `CA3d`, and `CA3dSource`; there is no fixture
provider or mixer. SDL output tests use process-local `SDL_AUDIO_DRIVER=dummy`.

```sh
cmake -S tests/native/a3d_resources -B build/a3d-resources-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DSDL3_DIR=/path/to/existing/SDL3/package
cmake --build build/a3d-resources-release --parallel 1
ctest --test-dir build/a3d-resources-release --parallel 1 --output-on-failure
```

Use a separate Debug directory; add `-DA3D_SANITIZERS=ON` in another Debug
directory for ASan/UBSan. Every CTest process uses `UBSAN_OPTIONS=halt_on_error=1`.
All builds, dependency builds and tests remain serial.

The 29 CTest entries cover:

- The three complete event suites from `a3d_notifications`, plus the authoritative
  stream crossing, replacement/removal, generation fences and waiter reentry.
  `a3d_stream_notification_tests` builds the existing full notification suite
  against an actual `ResManStreamBuffer` and `DalBufferInfo` submission fixture.
- Typed initialization with copied GUIDs, unsupported features, output failures,
  acquisition unwind and retry, and the actual `CA3dClassFactory::CreateInstance`
  activation route without recursive provider creation.
- All four resource modes, stable cursor kinds across mode changes, eight-voice
  software capacity, concurrent capacity admission, priority stealing, loopers,
  pool acquisition failure propagation, expiry across tick wrap and legacy notify
  rejection. No hardware capacity or vendor identity is synthesized.
- Source-byte submission accumulated across both real output lock spans and
  multiple source laps; a second update without output consumption submits zero
  bytes. Natural STOP waits for submitted audio to drain. Source format/control
  metadata is owned, duplicate PCM storage is shared with one writer, and native
  stream DS/3D/DAL views use the same canonical IUnknown.
- Deterministic final-stream-release/shutdown overlap, wrapped and reused-ring
  nonlooping tails, concurrent mode changes/creation (also checked with TSan),
  and actual local-factory parent allocation failure/unwind.
- Actual parent/source/duplicate acquisition failure, source QI retention,
  `held_source_after_parent` and `held_buffer_after_source`. The lifetime fixture
  observes the source's actual owned buffer via fixture-only `-fno-access-control`;
  it does not replace production ownership. A real resource-worker notification
  wakes a waiter that queries, replaces/removes, stops, closes and performs final
  buffer release. Final release joins workers and quiesces callbacks before zero
  owner/event/registration/wait/call counts are checked.
- An independent Python UUIDv5 derivation for the private software capacity IID.

Test-only failure hooks inject failure after actual acquisitions. They are
compiled out of the production graph. Intermediate source unwind comparisons
ignore only transient callback activity; final shutdown still requires every
counter to be zero. Full module export admission/unload, public acoustic feature
qualification, physical audio and game acceptance remain separate gates.

The render notification suites remain in `../a3d_notifications`; both owners use
the same authoritative test source. Resource-specific ownership, tail, queue and
pool tests live here without copied render crossing/fence/reentry test bodies.
The resource-mode race can be checked in a separate Debug build with
`-DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"` and
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread`, running only
`concurrent_resource_modes` with `TSAN_OPTIONS=halt_on_error=1`.


The managed-output regressions use real provider-created `ResManStreamBuffer`
objects and a real `CA3d`/`IA3dSource202` consumer. Only the SDL put boundary in
`native_output.cpp` is redirected, through this fixture's source-local compile
definition, to `output_faults.cpp`; successful puts still use real SDL. The
production graph has no injection hook or added source membership.

Persistent copy failures and terminal mixer admission errors must reach public
GetStatus/Play, including pending buffers without assigned streams, and public
CreateSoundBuffer/DuplicateSoundBuffer. The actual resource worker publishes its
latest tick status under the resource mutex. Managed buffer status returns zero
flags on error; source status propagates that error without adding the waiting
flag. Deferred source Play checks health and uses the existing
`A3DERROR_FAILED_PLAY` mapping. Successful SDL retry restores public operations.
Both regression cases check final zero owner/event counts after healthy teardown.

`virtual_source_progress` and `virtual_source_promotion` use ten managed sources,
stop the actual resource worker, and supply controlled `TickNative` timestamps.
Eight voices remain rendered. Excess finite sources complete and emit STOP once;
loops advance, wrap, seek, stop/replay and cross notifications while virtual.
Priority promotion writes PCM from the progressed position, and demotion resumes
virtual progression from the existing submitted cursor. No additional voice cap
is imposed. The dummy SDL output worker remains active; there is no physical device.

The source-local fallback adapts the retained `EMUBuffer` timing rule: fixed
11 source bytes/ms, independent of frequency/pitch, including its DWORD-rollover
one-millisecond adjustment. Sub-frame bytes are carried until a complete source
PCM frame is available. A new virtual interval or a pending seek begins at the
next resource update; demotion starts its interval at the assignment tick.
This restores logical progression at the native boundary, rather than reactivating
the legacy DAL/QI/timing-ring prefetch pipeline or claiming exact old pipeline
latency. Notifications use virtual logical advancement while no rendered stream
exists and retain native stream-submission semantics when rendered.

The resource worker still waits 10 ms and updates assignment after the retained
100 ms interval. This is separate from the native output worker's 20 ms poll,
8192-byte whole refill and strict 4096-byte low-water behavior.
