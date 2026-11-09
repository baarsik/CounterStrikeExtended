# Portable BMP records

`legacy_bmp_records.h` describes the little-endian 14-byte file prefix, 40-byte
information header and four-byte BGR palette entries without a Windows header
or a native `long`. Windows continues to use its SDK declarations in production.
The decoding bodies in `bitmap_win.cpp` and `vgui_int.cpp` are unchanged.

Configure this directory independently with CMake/Ninja and run CTest. On
Windows, `CSE_BMP_WINDOWS_REFERENCE=ON` runs the extracted production bodies with
actual SDK records; the default runs them with the portable records. The record
fixture also checks every field offset/width and record alignment against the
SDK. `CSE_BMP_SANITIZERS=ON` enables ASan/UBSan with Clang/GNU.

The loader fixture injects memory files and allocators. `FS_Read` returns bytes,
matching `filesystem_internal.cpp`. It exercises 36 authored 8-bit images and
nine aligned-width VGUI images, including palette and row orientation. Compare
the hex payload lines and numeric metadata; Windows C text streams use CRLF.
No game, window, font, filesystem provider or device is executed.

The fixtures preserve two existing limits. `LoadBMP16` currently rejects the
14-byte prefix because it compares a byte-returning read with one; its rejection
is recorded rather than repaired here. VGUI's existing pitch calculation is
measured only at aligned widths. Malformed input safety and either decoder
repair require a separate change.
