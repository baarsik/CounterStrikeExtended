# Source-built dependency configuration fixtures

Build explicitly with CMake/Ninja in Release or Debug. This directory uses the
runtime's cleared configuration defaults and the real pinned SDL3/mpg123 wrappers.
It does not register provider execution with CTest.

- `audio_conversion_probe <absolute SDL library> <output>` loads one owned library
  per process and converts 576 authored format/channel/rate cases: U8, S16LE and
  F32LE; mono/stereo; 11025, 22050, 44100 and 48000 Hz. The seeded input includes
  4097 frames to exercise conversion tails. Its little-endian diagnostic output
  records each configuration and exact payload. Compare complete output before
  and after a compiler-policy change, on the same CPU/OS. It opens no device.
- `decoder_fixture <sweep.mp3> <non-MP3 file> <output>` reuses the actual decoder
  test. Compare its 176400 bytes against `tests/music/pcm-reference.json`.
- `audio_stream_fixture` reuses the actual dummy-output lifecycle test: failed
  driver/stream creation, shared subsystem references, pause and repeated close.
  It explicitly selects the dummy driver.

Provider runs require an isolated authorized environment. These fixtures do not
qualify physical audio, microphones, voice, every media file or full gameplay.
The converter's dynamic loader is the production module loader; it does not load
multiple SDL images into the same process or claim repeated-unload safety.
