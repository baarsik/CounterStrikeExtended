This is an unselected data-only candidate and independent pure-transfer gate.
It is not the native font provider, a Surface binding or a capability promise.
The required CseVgui1QualifiedFontProviderV1 remains undefined.

make_owned_font.py writes a small SFNT from authored rectangles/triangles and
explicit CP1252 mappings. It reads no existing font. The Windows reference uses
unchanged extracted production constructor/raster/ABC bodies on that same memory
font, with checked DC/DIB/font ownership and paired Add/RemoveFontMemResourceEx.
It records smoothing enable/type/orientation/contrast and three draw orders.
Rendered BGR/RGBA buffers are outputs of authored geometry, not system font files.

The first generator's zero-contour glyph record was incomplete: GDI accepted the
font but space/tab ABC queries failed and the whole table used average widths.
That source/input/reference is preserved in ART. The corrected empty glyph has
equal loca offsets and supports the ordinary whole-byte ABC query. Current input
SHA256 is 069885a3e451df7439b6ae8279e942b22bf4017468e4868dd94ae936bdf2659a.

The new source requires explicit strict FP and NoEH. It checks native-width size,
metric and canvas representation before narrowing. The transfer layer preserves
the exact source float/double expressions, legacy B/G/R and weighted alpha,
nonzero pen, clipping and outside-region bytes. Its view has explicit capacity,
nonoverlapping spans and signed-pitch semantics. Exhaustion under the inherited
NoEH allocation policy is fatal; ordinary decoding/representation errors use
statuses. Address/undefined instrumentation covers owned source and fixture;
the pinned dependency archives themselves remain unchanged/uninstrumented.

HarmonyCandidate copies the font bytes and owns its own FT library/face/cache and
persistent canvas. It only implements an ordinary positive cell-height regular
width0/angle0 candidate. It uses the pinned existing Harmony geometry and checked
interpreter configuration. Unsupported negative FT bitmap pitch is reported,
not guessed. No CoreText, discovery, system font, VFS, shared registration,
Surface, UI or public Font object participates.

Candidate creation and data generation passing is NOT glyph equivalence.
compare_candidate.py performs an exact comparison and returns failure for the
observed coverage differences. The first ordinary corpus matches metrics, all
glyph IDs/CP1252 cells and ABC values but has different pixels. That mismatch is
frozen; no glyph algorithm, resource, filter/build option, width/style/rotation
gate or tolerance was changed afterward. A later reviewed native visual policy
is separate from the game's exact data/gameplay compatibility requirement.

The pure pixel layer is qualified independently: compare the complete RGB domain
with extracted actual expressions and six linked-MSVC output digests, and replay
captured GDI BGR buffers through the new implementation against exact RGBA bytes.
That isolates value/transfer fidelity from glyph-coverage differences and
post-flush/DIB sequencing. Lifetime/malformed/high-size/error controls do not
allocate giant buffers or use a native service. Root's four frozen adapter files
and prior component ASan reload split are untouched.
