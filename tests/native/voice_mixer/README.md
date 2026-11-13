# CoreAudio microphone mixer component

`voice_mixer_controls_coreaudio.{h,cpp}` implements a private mixer provider.
Full caller/release integration and real hardware qualification remain separate.
Windows mixer source and behavior remain unchanged.

The native factory copies a typed per-instance SDK/allocator services table.
It returns an owned object even when no hardware capability is available;
malformed services or object allocation failure return null. Temporary buffers
and property-owned CF objects are released within each call. Release consumes
the object once, using its copied allocator/context. Context must outlive all
calls and the object. Owner calls are serialized; no OS listener, worker,
mutable global registry, static cleanup or capture stream is introduced.

Each operation resolves the current default input and verifies device liveness,
input stream direction, complete nonoverlapping channel coverage and configuration
extents. Additional streams need not have microphone terminal classifications,
and additional source choices do not exclude an otherwise available control.
The complete current data-source ID array, when present, is snapshotted and
compared during revalidation. The provider does not identify or choose a new
physical microphone source. `SelectMicrophoneForWaveInput` retains the original
macOS successful no-op and performs no property access or source write.

Volume reads input channel 1 as the original macOS implementation did; a
master-only device remains readable if channel 1 has no volume property. A
failed or malformed channel read does not substitute another scalar. Volume
writes prefer a writable input master; otherwise they use the device's preferred
input stereo channels, including when the master is read-only. Both advertised
elements must have actual writable input scalar properties; no channel-number
cap is added to HAL's preferred pair.
Duplicate preferred channel IDs are written once. The existing mono-channel
fallback remains available when there is no preferred-stereo property. No
averaging or all-channel volume write is introduced. Direct normalized Float32
values retain 0..1 semantics without WinMM transport quantization. Invalid/nonfinite
volume requests fail. Monitoring MicMute is the inverse of hardware play-through
in its own scope; input capture mute and output master mute are not substitutes.
Boost requires an explicit input generic/custom BooleanControl, main or sole-channel element,
actual value property and case-sensitive Gain/Boost/+20d name rule. Standard
mute/jack/solo/phantom/phase/clip/talkback/listenback classes are excluded. The
last matching control is used, as in retained WinMM. This limited driver-name
heuristic is documented; no portable +20dB selector, dB-range substitution or
software gain is invented.

Get commits its float only after complete validation and a second matching
capability resolution. Set checks writability and scalar extents for both stereo
properties before writing, then resolves the same device, selected source array
and control again. It revalidates before the second channel write as well.
Changing array sizes have a two-attempt bound. Stereo writes are separate HAL
requests: failure returns false, but the first channel may already have changed.
There is no rollback or retry against a newly selected device/source. These checks
cannot make external hardware topology changes atomic; successful OSStatus means
the driver accepted the Set request, and
actual property changes can be asynchronous. No synthetic cached value masks a
failed or unavailable hardware operation.

## Current follow-up and historical evidence

The 2026-11-13 follow-up restores behavior established by offline original
`hw.dylib` analysis: channel-1 volume reads at `0x104780`, writable-master then
preferred-stereo writes at `0x104850`, and successful no-op source selection at
`0x104760`. Its fixtures were edited to reconcile the old singleton-source,
all-microphone, read-only-master and stereo rejection expectations. **This
follow-up was reviewed statically only. No tests, builds, compiler probes, target
execution or device calls were run.**

The archived pre-follow-up record reports Mac ARM64 Release (`-O2`), Debug
(`-O0 -g`) and ASan+UBSan Debug each passing 8/8:
factory/service-copy, main/sole volume, endpoint binding, monitor/boost,
singleton selection, allocation/CF/property failures, topology races and
independent owners. Every instance uses explicit fake services. Real SDK wrapper
bodies compiled, but their native symbols resolved to abort guards in the fixture;
no real HAL framework was linked or called. Pure CoreFoundation objects were used
for CF ownership/type controls. Actual objects/context addresses exceeded 4GiB.

That archived record also contains three separately compiled negative variants:
untyped input accepted as mic, readonly-main bypass and omitted write-size
validation. The untyped-input and read-only-master restrictions are superseded
by the original macOS evidence above and are not requirements of the current
provider. Original missing-factory, monitor, selector and write-width RED logs
remain frozen historical artifacts. The earlier whole-WinMM reference results
are unchanged. None of these archived results validates the follow-up source.

For future authorized execution, from the repository root:

```sh
cmake -S tests/native/voice_mixer -B build/voice-mixer-component-macos-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/voice-mixer-component-macos-release --parallel 2
ctest --test-dir build/voice-mixer-component-macos-release --output-on-failure -V
```

Use a separate Debug directory/configuration; add `-DCSE_MIXER_SANITIZERS=ON`
for a separate sanitizer build. `CSE_MIXER_MISSING_BASELINE=ON` selects an
explicitly failing test baseline only, never a product provider.

The final index is `voice-mixer-v1/implementation-v1/packet.json` under
`/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108`.
The actual runtime is macOS 27.0.1; deployment is 14.0, not a macOS14 runtime
receipt. This component does not qualify real HAL properties/driver metadata,
physical permissions/devices, driver-specific stereo/source layouts, Steam
endpoint correspondence, VoiceTweak/game
integration, voice delivery or audibility. Mac integration requires
CoreAudio/CoreFoundation, precise FP/no-EH+unwind policy and mixer release while
its module code is alive. Windows retains WinMM.
No AudioHardwareService/AudioToolbox API or virtual-main selector is used.
