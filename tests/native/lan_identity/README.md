# macOS legacy LAN identity

Run `test_identity.py --source /absolute/path/to/src/engine/sys_dll2.cpp
--evidence /absolute/fresh/receipt` on macOS with Clang. The fixture extracts the
actual native `Sys_GetCDKey` function and production CRC32 routines, then builds
with ASan/UBSan and controlled hostname/DNS calls. An independent Python zlib
oracle checks normal and nonterminated hostnames; the macOS branch must never
call DNS. A second build selects the unchanged legacy branch after system
headers have been included and checks its IPv4 formatting.

This legacy LAN identifier is separate from Steam ticket authentication. It is
hostname-based: equal hostnames collide and renaming changes it. The existing
random fallback when `gethostname` fails is preserved; its controlled test checks
formatting and requested random bounds, not real fallback stability. These
component checks do not establish multiplayer authentication acceptance.
