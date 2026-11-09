This is a read-only production design/reference fixture. No native font backend
is implemented or wired. The detailed proposal and evidence live in
ART/vgui-font-services-v1/DESIGN.md.

The Mac targets compile actual public/private VGUI1 Font headers and the existing
VGUI2 private service headers; they construct no fonts and execute no provider.
Compiler record-layout output records native Font's existing pointer fields and
getWide contract. The original generated Mac Windows-Font source still fails at
windows.h and that RED is retained.

On Windows, controlled_gdi includes the actual production win32/Font.cpp after
real SDK declarations and replaces only its GDI calls with deterministic owned
bindings. It also compiles actual common Font/fileimage/vfontdata sources. The
fixture records constructor inputs, width/angle/style/charset, metrics/ABC
failure fallback, exact RGBA bytes/tab handling, byte string measurement, cached
ID-1/reset quirks and existing platform borrower lifetime. It does not load or
call the real GDI font provider; all six PE import inventories omit GDI32.
Resource handles in this fixture are controlled owned tokens, not OS handles.

windows_reference.py freezes its input source and builds/runs x86/x64/ARM64
Release/Debug with /W4 /WX, keeping compiler environments in RAM. It does not
operate a desktop. The actual game-file inventory is a separate read-only ART
script; font and atlas bytes are hashed in place, not copied into the repository.

No CoreText/FreeType discovery/raster/provider execution, game, UI or hardware
runs in this design packet. The new native adapter, exact-face availability,
resource bootstrap, native style parity and actual consumer atlas/layout gates
require the parent's next bounded implementation approval.
