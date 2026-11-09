# Owned filesystem ABI qualification

This fixture compiles the provider and all three retained consumer declarations.
It hashes the 54-method `VFileSystem009` public prefix using normalized calling
shapes, virtual slots, handle widths, enums, and ownership revision. Provider
private virtual methods are outside that consumed prefix.

The production gate requires the owned provider role, public-prefix contract,
and compiler call profile before invoking `CreateInterface`. Supplied factories
must originate from that same checked image. Generic third-party module loading
keeps its existing behavior. The borrowed singleton and opaque file handles are
released through their owner; the boundary exchanges no STL containers.

The actual provider test is enabled with `CSE_FS_ACTUAL_MODULE`. Build
`tests/native/filesystem` first and pass its `runtime/filesystem_stdio` library to
this fixture. Use C++14, matching architecture/configuration, and macOS deployment
minimum 14.0. The four CTest cases cover mismatched/missing descriptors with zero
factory effects, all real header views, the real provider export/factory, and a
pre-epoch timestamp called through both actual declarations. That last case
reproduced the LP64 signed32-versus-signed64 mismatch before the consumer fix.

On Win32, compile-time assertions require cdecl factories, thiscall ordinary
members, and cdecl variadic members; unsupported `/Gr`, `/Gz`, and `/Gv` profiles
are rejected. The ABI does not impose unrelated CRT, RTTI, or STL policies.

For Apple ASan/UBSan, instrument both the provider and fixture. The mismatch
fixture retains its independent instrumented images until all checks finish,
then unloads them: Apple's sanitizer retains image-global metadata across
repeated unload/reload address reuse. Real unload behavior is also exercised by
the separate filesystem lifecycle suite.

These checks qualify this interface boundary, not full game, provider, or
external-server behavior.
