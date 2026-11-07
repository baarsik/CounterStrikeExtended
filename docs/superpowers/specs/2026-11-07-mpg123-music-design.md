# Miles replacement with libmpg123 and SDL3

The owner selected libmpg123 and authorized implementation and VM1 testing on 2026-11-07.
Replace proprietary Miles music playback while preserving existing music commands,
track mapping, priming, looping, volume/fades, focus behavior, and Windows physical CD controls.

## Architecture

Retain CCDAudio as the engine-facing controller. Add a portable music backend owning
one mpg123 decoder and SDL3 audio stream. A bounded producer thread performs decoding
and file reads outside SDL audio callbacks. Control operations synchronize with the
producer; stop/shutdown join it before releasing decoder/device resources.
Queued file/prime commands own bounded copies of their paths; playback controls keep
FIFO ordering while periodic status work may coalesce. Private worker records may
grow to hold paths; exported client interfaces and wire/disk layouts remain unchanged. Preserve
returned PCM at EOF, drain queued audio before completion, and explicitly handle
format-change/error results. Decode at the source sample rate; SDL handles device conversion.
Use opaque pointers, size_t lengths and mpg123 int64_t seek APIs without narrowing.

Build pinned upstream mpg123 1.33.7 as a shared library from an unmodified, hashed
source archive. Preserve complete upstream license/source materials. CMake and
standalone MSBuild must both build/stage the dependency in Debug/Release, and generated
target definitions remain synchronized through tools/translate_msbuild.py.
Remove active Miles imports/artifacts and revise dependency documentation/qualification.

## Approaches

The owner selected libmpg123 over dr_mp3 and SDL3_mixer. Use direct SDL3 music output;
feeding music into the existing sound-effects mixer would expand the Windows audio migration.
Xash3D FWGS e9b94dc1 is an architectural reference only; do not copy its GPL audio glue,
custom stub or trimmed Waf build. Use mpg123 public APIs and upstream build machinery.
Keep physical CD support and DirectSound/WaveOut unchanged in this task.

## Acceptance

Real decoder/stream tests cover complete final PCM, looping, pause/resume, replacement,
invalid inputs, volume clamping, subsystem ownership and repeated shutdown.
VM1 builds all four presets and checks native Windows x86 behavior and import closure.
Exercise CS and Half-Life with a fixture excluding Miles, and capture actual music output
and live module evidence. Report physical-CD/device cases not exercised.
Future native x64/ARM64 readiness is architectural; full runtime ports are out of scope.
