# Native browser process transport fixtures

The transport uses the pinned SDL3 process APIs and two nonblocking pipes. It
executes no CEF, Steam, game or network session. The native process fixture uses
only an owned echo child and inherited standard error; it never invokes a shell.

Configure with Ninja and an architecture-matched compiler. Set
`CSE_BROWSER_SDL_PROVIDER` to the qualified SDL3 runtime and, on Windows,
`CSE_BROWSER_SDL_IMPORT_ARCHIVE` to its matching import library. Run CTest after
building. Omitting the SDL3 provider builds the offline wire fixture alone.
`browser_process_tests --peer <other-architecture-fixture>` tests a differently
sized process ABI on the same OS.

The 24-byte header contains CSEB magic, a 16-bit protocol version and message kind,
then 32-bit operation, browser ID, request ID and payload length. Every integer
has an explicit little-endian encoding. Payload strings are byte-counted. Native
pointers, structures, C++ objects and function addresses never enter the stream.
Consumers choose a payload bound before decoding any body allocation.

Checks cover exact bytes, every frame split, coalesced and empty messages,
truncation, invalid headers/booleans/lengths, large simultaneous transfers in both
directions, command ordering, Unicode and quoted arguments, failed startup,
duplicate ownership, child failure, protocol failure and recovery. Closing input
cannot discard queued data. Finish flushes accepted messages while draining
replies, closes input, drains EOF and reaps the child; no worker is forcibly
terminated. Controllers must detach UI callbacks before discarding shutdown
replies.

These sources are prepared for the owned native HTML provider. They are not yet
selected by a production runtime manifest or connected to the CEF host. Passing
these fixtures does not qualify browser behavior or a complete native game.
