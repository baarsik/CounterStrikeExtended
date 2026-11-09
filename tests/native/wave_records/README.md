This fixture extracts the actual `COM_GetApproxWavePlayLength` record/function and
the actual voice WAVE reader/writer into a controlled, in-memory filesystem.
It opens no files or devices at runtime. The filesystem rejects short reads so
uninitialized bytes cannot become comparison evidence.

It checks the 24-byte Win32 `fmt ` record and its offsets, exact 52-byte PCM output,
32-bit signed output words, file ownership, missing files, rejected signatures,
duration arithmetic (including retained unsigned wrap), and the existing voice
reader's data offset. Supported target CPUs are little endian.

The retained voice reader consumes an extra DWORD after the data length and reads
PCM at offset 48; the writer starts PCM at offset 44. This migration preserves
that existing difference. The reader fixture supplies complete bytes at its
actual offset; it does not claim that the existing writer/reader round-trip or
truncated-input handling is correct.

Run with CMake and Ninja using `CMAKE_BUILD_TYPE=Release` or `Debug`.
`CSE_WAVE_SANITIZERS=ON` adds ASan/UBSan. Source overrides
`CSE_WAVE_COMMON_SOURCE` and `CSE_WAVE_VOICE_SOURCE` allow the preserved Win32
implementation to provide an independent byte/value reference with MSVC.
These are record/function checks, separate from recording, playback and audio
device acceptance.
