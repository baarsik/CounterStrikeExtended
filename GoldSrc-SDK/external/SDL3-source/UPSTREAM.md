# SDL3 source provider

Unmodified SDL 3.4.20 release source:
https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20

Archive: https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
SHA-256: `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`

The archive digest matches the official release asset digest. All 86 bundled
`external/SDL3/include/SDL3/*.h` files match this source archive byte for byte.
The archive contains its upstream zlib license and notices; no source patches
are applied. Shared provider builds use the target compiler, retaining upstream
platform feature defaults. Static libraries, SDL tests/examples and installation
rules are disabled; this does not disable runtime subsystems.

`CSE_SDL3_FROM_SOURCE=ON` selects this provider. The pinned Win32 binary remains
the default during reference qualification. Native dependency presets select
the source provider. This is dependency compilation support, not game acceptance.
