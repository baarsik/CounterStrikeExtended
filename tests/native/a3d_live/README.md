# Bounded A3D output/coexistence handoff

This is an explicitly invoked diagnostic fixture. It is not part of default runtime output or broad CTest discovery.

It admits and loads the actual owning A3D dylib, exercises the public IA3d3/NewSource/Play/Flush producer with resource mode OFF, supported nonzero PCM and cursor-preserving Stop/Play, invalid-GUID initialization failure followed by default initialization, repeated final release and actual loader absence. The selected-GUID public API remains open: the module has no public producer for its private process-local device token. No fake GUID, private constructor call or SDL device redirection fills that gap.

The executable compiles exact extracted SNDDMA_Init/Shutdown/LockBuffer/UnlockBuffer/GetDMAPos and S_BlockSound/UnblockSound bodies with the actual `sdl3_audio_backend.h`, plus unmodified `mpg123_music.cpp` and `mpg123_decoder.cpp`. `prepare.py` extracts the actual `dma_t` declaration and validates every field type/offset, size/alignment and function signature under the owning hardware-engine compile recipe. This is source-selected engine DMA/music coexistence, not an engine A3D consumer or full game launch. It checks ongoing DMA position and music playback after A3D teardown, then the reverse order. Music uses the pinned mpg123 source package's `sweep.mp3`.

`a3d_live_bare` has no interpose records. `a3d_live_instrumented` links a dedicated test observer library that transparently forwards the real SDL open/put operations, observes the actual A3D output stream and captures a bounded preallocated pre-conversion PCM prefix using lock-free atomics. No callback lock, allocation, logging or file I/O occurs. Capture is drained only after provider teardown quiesces output. Provider path/function-address/loaded-image checks ensure one canonical staged SDL provider. The wrapper records binary/module/provider/track hashes before and after.

An explicit `--inject-put-failure` scenario returns false once per A3D output lifetime. It is injected transport failure, not physical removal. Subsequent cycles create a new provider; they do not demonstrate Restore support. Ordinary calls are forwarded unchanged. Actual physical audibility is never inferred from queueing, nonzero captured PCM or cursor progress.

Configure/build serially against a completed fresh graph:

```sh
cmake -S tests/native/a3d_live -B NEW_FIXTURE -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DA3D_GRAPH=ABSOLUTE_GRAPH
cmake --build NEW_FIXTURE --parallel 1
python3 tests/native/a3d_live/run_probe.py --graph ABSOLUTE_GRAPH --fixture NEW_FIXTURE \
  --profile Release --evidence NEW_EVIDENCE --instrumented
```

The wrapper defaults to an asserted process-local SDL dummy driver. An actual-output run requires both `--root-authorized-live-default` and `CSE_ROOT_LIVE_OUTPUT_AUTHORIZED=1`; only Root may dispatch it after the reviewed output-only Speakers wrapper has selected and verified the approved route. The wrapper must restore the original stable output UID and leave input, system output, volume and mute unchanged. No route manipulation is included here. Do not retry the unchanged RODE HAL IPC stall, touch services/security/consent, open an input device, or infer physical audibility without a human observation. A case times out after 20 seconds and is not automatically retried.

Each evidence directory is newly created and never overwritten. Bare, instrumented and injected cases must use different directories. Hardware unplug/recovery, OEM semantics, microphone/voice, public selected-device enumeration and genuine game routing remain separate gates.

The initial low-level DS probe captured zero-gain output because it submitted no source producer controls; it is not nonzero-PCM acceptance. A subsequent source probe left the default streaming resource mode selected and incorrectly treated its submitted cursor as continuous render progress. Final cases explicitly select resource mode OFF and use the public source producer. No production gain defaults, DSP or stream cursor semantics were altered to satisfy the fixture.
