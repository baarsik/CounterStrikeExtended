# Exact FixedSys source-correspondence probe

This is a VM1-only, console/offscreen reference. It creates only the project's
`FixedSys,18,0,0,400,no-styles,ANSI` request with the unchanged extracted Windows
Font.cpp constructor/method bodies and checked fixture-owned GDI cleanup. The
fixture does not establish VGUI class ABI, bootstrap, provider or UI acceptance.

It asks the selected HDC's documented DirectWrite interop for a source, queries
one exact registry value, then tests the explicitly named `Windows/Fonts/vgafix.fon`
candidate if the value is absent. That fallback filename is a hypothesis until
metadata/raster correspondence is measured; it is not a loader-path proof.

Only metadata, hashes and per-byte digest comparisons leave VM1. Font files,
font resources, tables, glyph images and dump bytes are never saved/exported.
The candidate's bytes live in a local vector until all FT faces are destroyed.
No font is installed/registered, no setting is changed, and no window/input,
CoreText, native Mac font service, game or network is used.

Use `windows_probe.py --inputs <frozen-inputs> --workspace <fresh-owned-path>
--output <fresh-receipts>`. It builds MSVC x86/x64 Release/Debug with `/W4 /WX`,
C++14, strict FP and the pinned FreeType2.14.3 archives. The frozen pixel-transfer
TU retains its own NoEH policy. Compiler environment is kept in RAM. Each phase
has a 90-second subprocess timeout; each profile checks the 4GiB disk floor.

`analyze.py` reads only receipts and the earlier FixedSys reference; it verifies
all256 prior GDI hashes, exact ABCs/advances, the finite mismatch set and profile
identity. The `cp1252-unicode` FT route is an explicit negative control: WinFNT's
sole unencoded charmap must be selected explicitly and addressed by original
byte, not treated as a Unicode cmap. Neither route is a production policy.

The frozen final report is under the separately owned artifact packet
`vgui1-fixedsys-source-correspondence-v1`. A matching bitmap font program is a
concrete local-only comparison option, not redistribution permission, selected
product dependency, Courier replacement, qualified factory or readability pass.
