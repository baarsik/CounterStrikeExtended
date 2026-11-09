# Native fallback voice capture component

`engine/voice_record_sdl.{h,cpp}` implements the private
`CreateVoiceRecordSDL(sampleRate, policy)` factory without changing IVoiceRecord.

The two policies intentionally retain different Windows behavior. WaveIn keeps
3840 bytes, treats the capacity as samples, and returns the newest requested
window. Its stopped queue can be read once, and restart before stopped Idle
preserves it. DirectSound keeps one second, treats capacity as bytes, and retains
DWORD cursor/event aggregation and the odd wrapped short-pointer copy effect.
Both discard on zero capacity. Negative capacities, positive null destinations
and unsafe computed ranges return zero without committing a read or writing.

The recorder owns one SDL audio reference, one simplified logical recording
stream, and one bounded queue. The stream requests mono S16LE at the supplied
rate, verifies the app-side format, and resumes once. Callback reads drain a
finite converted-data snapshot through a 256-byte scratch buffer. No component
allocation, device operation or logger runs there. Malformed reads latch the
first failure. Only after failure, SDL_ClearAudioStream prevents later callbacks
from accumulating SDK data before owner Idle. The pinned implementation uses
its recursive stream lock and clears copied queue/conversion state without
tearing down the device. Healthy zero-progress reads end that callback.

Owner operations are serialized by the engine; only the SDL callback is
concurrent. Close pauses without holding the stream lock, detaches under SDL's
stream lock, destroys/quiesces the stream, frees its queue, and releases its own
reference. Pause failure still takes the detach/destroy path. A failed Init
returns null after releasing successfully acquired resources. No global hints,
new worker or selectable fake provider are introduced.

## Fixture boundaries and results

The actual production CPP is compiled as a separate object with C++14 and no
C++ exception handling. AppleClang additionally uses `-funwind-tables`, matching
the retained native policy. Fixtures and their fake shared SDK use ordinary EH.
Every SDL C function has the real pinned SDL3.4.20 declaration. The fake shared
library is the only SDL implementation linked; no recording provider executes.
Legacy reference tests compile the whole unchanged WaveIn/DSound/CircularBuffer
bodies against real Windows SDK types with explicit fake multimedia bindings.
Historical OpenAL is tested separately and is not the selected provider.

| Actual component configuration | Cases | Result |
|---|---:|---|
| Mac ARM64 Release | 7 | pass |
| Mac ARM64 Debug | 7 | pass |
| Mac ARM64 ASan+UBSan Debug | 7 | pass |
| Windows x86 Release/Debug | 8 each | pass |
| Windows x64 Release/Debug | 8 each | pass |
| Windows ARM64 Release/Debug | 8 each | pass |

The Mac runtime was macOS 27.0.1, not macOS 14; the deployment target is 14.0.
Every configuration performs 16 real fixture DLL/dylib cycles. Its typed ordinary
SDL callback address must belong to the loaded module. Release waits while
that callback is blocked inside a controlled read, then detaches/joins before
unload; no callback, stream, queue or audio reference remains. This does not
qualify the engine's exceptional-exit guard, which must explicitly release its
recorder before unloading the engine module.

Controls include allocation/init/open/format/resume failures; signed16 bytes;
newest/drop/wrap/cursor behavior; Wave stop/restart/Idle and DS no-op controls;
zero/negative/null bounds; odd DS destination canaries; actual object/output
addresses above 4GiB on native64; odd/partial/zero/oversized/negative reads;
finite snapshots; repeated callbacks after terminal failure; in-flight close
with successful/failed pause; repeated starts/stops; and independent audio owners.
The no-allocation check concerns component code and explicit boundary calls;
SDL conversion workspace and physical-device real-time behavior are separate.

## Reproduction

On Mac, from the repository root:

```sh
cmake -S tests/native/voice_capture -B build/voice-capture-component-macos-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/voice-capture-component-macos-release --parallel 2
ctest --test-dir build/voice-capture-component-macos-release --output-on-failure -V
```

Use a separate Debug build directory with `-DCMAKE_BUILD_TYPE=Debug`. For a
separate sanitizer Debug directory, add `-DCSE_VOICE_REFERENCE_SANITIZERS=ON`.
`CSE_VOICE_COMPONENT_BASELINE=ON` is an explicitly failing missing-factory test
baseline only; it is never a production source or a successful provider stub.

On Windows, use a fresh VsDevCmd environment for each `-arch=x86`, `x64` and
`arm64` with `-host_arch=arm64`; configure the same leaf project with Ninja,
`CMAKE_CXX_COMPILER` set to that environment's `Hostarm64/<arch>/cl.exe`, and a
fresh directory per CPU/configuration. The repository MSVC defaults hook is
read, and the fixture supplies `/W4 /WX /fp:precise`, `/O2 /MD` or `/Od /MDd`,
with `/EHs-c-` only on the production capture object. It does not change root
policy or manifests.

The sealed Windows producer script and exact six configure/build/test commands
are in artifact `run-voice-capture-component-v3-windows.py` and
`voice-capture-component-v3-windows-matrix.json`. The physical standalone source
is `C:\CI\CSE-Native64-VoiceCaptureComponentV3-20261109\source`.
All inputs are hash-checked after copying. No live runtime producer is modified.

## Evidence and integration handoff

Artifact base is `/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108`.
The final index is `voice-capture-component-final-packet.json`. Key receipts:

- `voice-capture-component-v3-source-hashes.json`: 442 frozen Windows inputs.
- `voice-capture-component-v3-windows-matrix.json`: six cases and 36 PE images.
- `voice-capture-component-final-macos-matrix.json`: three cases and 15 images;
  actual capture-object dependency hashes match the frozen Windows closure.
- `voice-capture-component-v3-windows-cleanup.json`: no owned fixture processes;
  no scheduled tasks created. Workers are joined inside the fixture before exit.
- `voice-capture-reference-v5-windows-matrix.json`: independent legacy reference.
- Initial missing factory, read-policy, and terminal-clear RED/GREEN logs and
  fixture compile failures remain preserved alongside the final receipts.

Native builds select the SDL sources; Win32 retains its original capture
sources. Preserve explicit fallback ordering when changing that selection. The component has no cvar dependency. Keep
the existing Steam capture/compression path through its native facade; SDL PCM
is not submitted into Steam. The exceptional unload boundary must clear the
owner pointer and explicitly release once outside loader lock. The component fixture does not qualify complete caller or provider integration.

Physical permission/device negotiation, real conversion/hotplug, Steam capture,
codec and mixer providers, VoiceTweak, unchanged-server voice delivery and
audibility remain unqualified. The external fallback codec and incoming codec
selection need separate work. ARM64 Steam
provider acceptance is also separate. These component passes do not promote a
complete target, voice path or game runtime.
