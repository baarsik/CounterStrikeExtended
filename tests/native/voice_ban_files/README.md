# Legacy voice-ban file compatibility

Build with Ninja and run CTest. The two executables use the complete real CS/HL
ban managers and their own headers. They exercise the four-byte legacy version,
256 authored raw16-byte IDs, order, duplicate records, mutation and persistence;
new readers also check incomplete headers. All files live under CMake-authored,
marker-checked directories. No microphone, sound, network or game runs.

`CSE_BAN_SANITIZERS=ON` enables ASan/UBSan. `CSE_BAN_REFERENCE_ROOT` can point to
preserved `hl.cpp`/`cs.cpp` owners; only complete valid files execute on those
readers, never malformed legacy reads. Their one native-width compile warning is
lowered only for this diagnostic baseline. Current sources retain strict checks.
The optional `import` mode consumes another profile's authored canonical file and
checks exact decoding/re-encoding, without modifying its input.
