# Actual voice capture caller wiring fixture

The extractor copies complete current Voice_Init, Voice_Deinit and both owned
clear-before-Release helpers, plus real default-rate/sample-format declarations.
Generated bodies preserve conditionals and #line/source/body hashes. The real
voice_record_native.cpp is compiled unchanged on native64; fake SDL factory
returns controlled owned recorders without linking SDL. The x86 Windows case
is a caller-only reference with named factory seams, not native-delegate
production selection.

Real recorder, codec and mixer interfaces are used. Private CVoiceChannel/cvar
state is minimal caller scaffolding, not a layout/ABI proof. SteamUser provides
presence only; no Steam SDK function is called. Codec/module/filesystem/mixer/
print/VoiceSE calls are controlled seams. Voice_RecordStop and
Voice_EndAllChannels are explicit bounded seams; their complete behavior,
module-exit guard and provider internals are separate fixtures. Unexpected
recording/data/codec methods abort. No physical/provider/HAL/UI/network call or
real external codec/module load occurs.

Actual caller cases cover DS/Wave priority for zero/nonzero voice_dsound,
one retry only, both modes absent, default and changed sample rate, quality
forwarding to all actual channel iterations, Steam and empty-codec skip,
platform-native codec filename, disabled/VoiceSE/module/factory/codec/channel
failures, forced-mic gating, repeated initialization and once-only cleanup.
Both-recorder failure still permits receive/codec initialization in this source;
the test requires the capture-unavailable messages and a null recorder.
Unsupported forced-mic selection does not change initialization success.

Build from the repository root with this leaf CMake project, Ninja and a fresh
Release/Debug directory. Caller and native-delegate objects retain no-EH policy;
AppleClang uses unwind tables. Fake seams/tests use EH. Explicit Release uses
-O2 (/O2); Debug uses -O0/-g (/Od). CSE_VOICE_WIRING_SANITIZERS adds ASan+UBSan.
MSVC uses a fresh VsDevCmd environment per x86/x64/ARM64. Root production and
live producer files are read only. No root membership/build file is edited.

Final logs, source/body/command/image hashes and intentional counterfactual
controls are indexed under artifact voice-capture-wiring-v1. A passing caller
fixture does not establish actual Steam/codec/provider ABI, physical recording,
voice delivery/audibility, complete engine initialization or target acceptance.

Frozen results: all three Mac configurations pass the complete caller suite;
MSVC x86/x64/ARM64 Release/Debug each pass. Actual caller/delegate commands,
images, source/body hashes and import audits are in the final packet.json.
Three isolated generated-body mutations fail the priority, owner clearing and
native filename assertions. The first Windows attempt's fixture-only int-to-float
literal warning is preserved; the corrected V2 source is independently copied,
hash-checked and built. Source bodies are not normalized or rewritten.
