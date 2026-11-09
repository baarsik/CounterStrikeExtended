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
input stream direction, standardized microphone/headset/receiver terminal kinds,
complete nonoverlapping channel coverage and configuration extents. Arbitrary
default line-in is rejected. A mutable multi-choice source selector is ambiguous
and cannot authorize any mic capability or write. Missing selector is a fixed
endpoint; exactly one matching available/current item is unambiguous. Selection
returns false when no selector exists and writes only the verified singleton ID
when settable. No system default input is changed. No general mutable selector
mapping is claimed.

Volume uses a real input/main scalar. Only its absence permits fallback to the
positively bound sole microphone channel; readonly, failed or malformed main
properties cannot authorize a channel bypass. Direct normalized Float32 values
retain 0..1 semantics without WinMM transport quantization. Invalid/nonfinite
volume requests fail. Monitoring MicMute is the inverse of hardware play-through
in its own scope; input capture mute and output master mute are not substitutes.
Boost requires an explicit input generic/custom BooleanControl, valid element,
actual value property and case-sensitive Gain/Boost/+20d name rule. Standard
mute/jack/solo/phantom/phase/clip/talkback/listenback classes are excluded. The
last matching control is used, as in retained WinMM. This limited driver-name
heuristic is documented; no portable +20dB selector, dB-range substitution or
software gain is invented.

Get commits its float only after complete validation and a second matching
capability resolution. Set checks writability, resolves again and validates
write extent before calling the property setter. Changing array sizes have a
two-attempt bound. These checks cannot make external hardware topology changes
atomic; successful OSStatus means the driver accepted the Set request, and
actual property changes can be asynchronous. No synthetic cached value masks a
failed or unavailable hardware operation.

## Evidence

Mac ARM64 Release (`-O2`), Debug (`-O0 -g`) and ASan+UBSan Debug each pass 8/8:
factory/service-copy, main/sole volume, endpoint binding, monitor/boost,
singleton selection, allocation/CF/property failures, topology races and
independent owners. Every instance uses explicit fake services. Real SDK wrapper
bodies compile, but their native symbols resolve to abort guards in the fixture;
no real HAL framework is linked or called. Pure CoreFoundation objects are used
for CF ownership/type controls. Actual objects/context addresses exceed 4GiB.

Three separately compiled negative variants fail their intended regressions:
untyped input accepted as mic, readonly-main bypass and omitted write-size
validation. Original missing-factory, monitor, selector and write-width RED logs
are preserved. Existing whole-WinMM references still pass all six Windows
CPU/config cases in the earlier sealed packet; they were not rewritten.

From the repository root:

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
physical permissions/devices, mutable selector mappings, multichannel devices
without real main control, Steam endpoint correspondence, VoiceTweak/game
integration, voice delivery or audibility. Mac integration requires
CoreAudio/CoreFoundation, precise FP/no-EH+unwind policy and mixer release while
its module code is alive. Windows retains WinMM.
No AudioHardwareService/AudioToolbox API or virtual-main selector is used.
