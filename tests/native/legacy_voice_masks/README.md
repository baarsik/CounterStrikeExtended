# Legacy Half-Life voice masks

The fixtures import the selected `UpdateServerState` and `HandleVoiceMaskMsg`
bodies without rewriting them and use the real complete `CVoiceStatus`, bit-vector
and engine callback declarations. Construction is controlled; unrelated virtual
entries abort. The actual in-memory ban-manager methods and message reader are
used. No game, microphone, voice provider, network or persistent ban file is opened.

Seeded and single-bit messages cover all 32 player bits, debug text, missing player
identities, enable bytes, level absence, forced updates, ban-command bytes and send
decisions. The original Windows branch remains the reference. Native storage and
printf arguments must preserve its 32-bit masks; this is separate from audible
voice or external-server qualification.

Run `python3 tests/native/legacy_voice_masks/run_mac.py --art <new-artifact-directory>`
for Release, Debug and ASan/UBSan. Windows uses the same CMake fixture in an MSVC
command environment. Compare decoded output after the explicit CRT CRLF/LF
presentation conversion while retaining the original byte streams.
