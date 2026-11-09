This standalone component compiles the complete actual `tier0/vcrmode_posix.cpp` through the fixture.
Synchronous socket calls and stdio failure points have controlled bindings. It creates temporary files,
including one sparse file, and never creates a network socket or starts a game/provider/UI.

From the repository root:

```sh
python3 tests/native/vcr_posix/run_checks.py --phase green --output /tmp/cse-vcr-posix
```

It requires the existing Apple compiler/SDK and Rosetta for execution of the x86_64 profiles. It builds C++14
ARM64 Release, Debug and ASan/UBSan profiles plus x86_64 Release and Debug profiles. Each profile runs 38 cases.
Optional `--whole-tu-commands path.json` also replays pinned actual generated production compiler arguments,
redirecting object outputs and adding one separately identified sanitizer compilation.

The independent Python wire oracles describe v2 with literal event tags, little-endian fixed 32-bit fields,
binary64 time and explicit payload bytes. They neither import production constants nor use the recorder as
their expected output. Every architecture must record exactly the same 86-byte socket/generic/time packet,
67-byte control packet and 11-byte unnamed-generic packet, and replay the independent inputs.

Coverage includes fixed-width boundary values, exact EOF and empty recordings, failed open/seek/tell/read/
write/flush, native ssize_t bounds, immediate socket errno capture, returning error helpers, short reads,
negative/over-capacity sizes, malformed events/names/strings/addresses/game fields, and disabled hooks.
Native file extent/cursor arithmetic is tested above 8 GiB using a sparse temporary file and controlled
positioning; the runner releases that sparse file after the case. Command buffers retain their existing
`new[]` versus `malloc` ownership conventions. The tests do not force actual allocator exhaustion.

Malformed input reports the helper error, ends playback and returns failure; format/truncation errors use
EIO. A failed requested read region is zeroed, while bytes outside that region remain unchanged. Invalid
metadata rejected before a read leaves the caller buffer unchanged; game-message output fields commit only
after a complete valid payload. Native I/O errors retain their errno. No rejected replay calls a socket binding.

The source's public/shared enums, helper/trace interfaces and VCR function table remain unchanged. Internal
file positions are native-width and never serialized; private negative sentinels use signed storage. The
Windows-selected branch stays empty. This does not qualify recording interoperability with a Windows VCR
producer, a complete VCR implementation, or full runtime behavior. Existing platform stubs/null hooks,
unrecorded local time, caller-dependent recvfrom address lengths and the separate command-line builder remain
outside this component's qualification.
