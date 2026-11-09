# Actual owner / CPU text-scene fixture

This fixture defines a test-only qualified binding that observes then forwards the actual candidate provider. It
permits only the explicit FixedSys18/400 -> Courier/index0 role, with the exact pinned Courier program. It binds the
one pinned 6136-byte game Marlett resource through the actual owner and shared process-scoped registration service to
establish resource generations. It does not request Marlett glyphs. No production factory or source is changed.

The fixture checks pending construction/passive query, name-copy ownership, case-insensitive cache/retained cache-hit
ID behavior, Reset + current-generation-only global preflight, scoped Prepare of a retained borrower, duplicate binding,
all 256 ABC/raster records and all 128 sign-extended byte equivalents, actual multiline/empty/null getTextSize, and two
normal release/rebind lifecycles with stable public platform/ID. Calls to the real Surface binding abort. No real
registration failure is forced. Unexpected actual release failure retains its token/context and exits without automatic
retry or forced unload. Normal successful release and context destruction are required.

Authored scenes cover ASCII letters/digits/ambiguous shapes, CP1252 accents/punctuation/prices, aligned numeric columns,
native control placeholders/default blocks, explicit-length blank spacing, a baseline guide, selection/caret and
scissored partial glyphs. Every glyph comes through actual Font::getCharRGBA with a nonzero scratch pen. Text positions
use EngineSurfaceWrap::drawPrintText's A/width/C arithmetic. Cursor/selection arithmetic and the three-pixel text inset
come from TextEntry.cpp; its selection-background offset is characterized without correction. Actual getTextSize is
checked separately, preserving its newline/first-line behavior. Explicit-length control rows do not reinterpret newline
or NUL as layout commands.

PNG presentation uses an explicit CPU convention: nearest source pixels, byte RGB modulation and source-alpha over an
opaque background, with nearest integer rounding. It does not model GPU filtering, gamma/sRGB, color management,
display scale or framebuffer rounding. The 3x image is integer nearest-neighbor magnification. These are text scenes,
not an exported font program, table or glyph atlas. All 256 glyph records are metadata/digests only.

`run_macos.py` freezes the current production/test inputs (including root's exact owner-profile and test-membership
deltas), compiles Release/Debug/ASan+UBSan, checks all three images/dependencies/resources before execution, then runs
each with a 45-second external watchdog and fresh output directory. User authorization for the required macOS tests
has been relayed by root. No UI/game/window, clipboard, preference, device, network, inventory, leak or module-reload
test is run. Sanitizers cover owned source, not pinned static FreeType/dependency archives.

Passing means actual owner/candidate metrics, bytes, normal ownership and CPU-scene mechanics passed. Visual readability,
GDI glyph pixel equality, Surface/font-atlas/renderer/bootstrap and whole-UI acceptance remain open. In particular,
EngineSurfaceWrap's existing non-Windows `isprint` atlas filter is not exercised or silently fixed by this fixture.

Before each forwarded realization the fixture independently checks the same Courier locator path, PS name, index0
and program hash. The provider then performs its own normal lookup/read; before/after pins and complete byte equality
are checked. No other locator or font program is read if this exact guard fails.
