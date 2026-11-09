Compiles every actual `BufferWriter` method unchanged from the owning
`common/parsemsg.cpp` tail. Reader methods are outside this component. The whole
owning production translation unit must compile separately.

The message corpus checks exact empty/string/mixed-field bytes, sticky overflow,
used lengths and untouched guards across all small capacities. A separate
controlled `strlen` seam reports native lengths near 2/4 GiB and `SIZE_MAX` without
allocating large strings; the real transfer must reject those before narrowing or
adding a terminator. Ordinary corpus cases use the real strlen and all transfers
use real strcpy. No socket, provider or game executes. Equal corpus digests across
architectures are component evidence, not external-server acceptance.

Configure with CMake/Ninja in Release/Debug. Set
`CSE_MESSAGE_WRITER_SANITIZERS=ON` for the native sanitizer fixture.
