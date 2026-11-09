This fixture executes pure rectangle calculations. It does not link or call a
renderer, GL context, window backend, provider or game. `extract_bodies.py`
imports exact scalar methods/blocks from the two actual engine owners and pins
source/body hashes. Native helpers use the real canonical header. The video
state record comes from actual vid.h; VideoMode_IsWindowed is controlled.

`run_windows_reference.py` runs on VM1 with MSVC x86/x64/ARM64 Release/Debug. It
executes the actual selected IntersectRect branch plus imported scissor/UV and
screenshot-dimension calculations. Every OS pointer argument is valid. A second
explicitly controlled model imports the original native algorithm with real
Windows SDK legacy32 types to expose its empty/inverted rectangle RED. The
owning /fp:precise, /O1 or /Od, DLL CRT and x86 SSE2 flags are recorded. No full
runtime qualification is inferred from these extracted methods.

`check_reference.py --art PACKET` requires equality across all six actual OS
profiles and checks intersections against an independent unbounded-integer
geometry oracle. Its golden contains exact rectangle results and encoded float
bits, including unchanged UV outputs on rejected intersections, aliases, scissor
disabled behavior, touching/inverted rectangles and signed32 boundaries.

`run_mac_matrix.py --art PACKET` runs C++14 Release/Debug/ASan+UBSan comparisons
against that golden, an independent int64 oracle for 100081 wrapping differences,
and a C11 declaration probe. It then compiles both full owning TUs R/D using
the current root compile databases with only object/dependency outputs redirected
under ART. It does not rewrite or upgrade the root compiler policy.

`run_include_orders.py` checks actual common/port.h/cse_rect.h inclusion both ways
in C11/C++14, with Apple and LINUX definitions on the Apple host. That checks the
conditional header contract; it is not an actual Linux backend/OS qualification.
`check_windows_tokens.py` compares full owning TU/header preprocessing in 18
MSVC contexts against frozen owner includes and rejects a selected +1 control.
The token reader has numeric-separator, raw/quoted-string and unknown-report
controls in `test_cpp_tokens.py`. `run_negative_controls.py` rejects four copied
representation/arithmetic/body mutations without changing production sources.

Native null handling in SurfaceIntersectRect is retained; no Windows API NULL
call is made. The packet does not qualify all renderer coordinate additions,
texture allocation/upload arithmetic, screenshot file operations or the unused
InterpTCoord function. Runtime integration and build selection are checked separately.
