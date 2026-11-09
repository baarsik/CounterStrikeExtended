This is an unselected, compile-only preparation of a reviewed native font probe.
Building it executes no CoreText, FreeType face, registration, provider or UI
probe. No CTest or post-build execution is registered. A later explicit root
authorization is required before either runtime mode is launched.

`--discover-needed output.tsv` performs eight fixed requests over four VGUI1
families: Arial, Arial Narrow Bold, FixedSys and Marlett. It also checks only the
two exact PostScript identities already used by the frozen owner's measured
resolution profiles: ArialMT and Wingdings-Regular. It does not enumerate font
families or invoke the Unicode fallback/cascade API. Heights/weights come from
Scheme.cpp, the pinned valve/1152_textscheme.txt and exact owner profile tuples;
the scheme loader initializes an omitted FontWeight to zero. No other name can
be supplied on the command line.

For each exact lookup it records absence ordinarily, or the full resolved path
and face/family identity as hexadecimal UTF8/raw FT-name bytes, collection/named
instance index, owned in-memory program SHA256 and glyph count. It opens the
reported index from that owned snapshot and verifies the PostScript identity,
then checks repeated discovery stability. It saves no font program/table bytes.
Read snapshots are limited to 64 MiB; candidate canvases to 16 MiB. These are
probe resource bounds, not new supported-production font limits.

Only regular weight400, non-symbol, face-index0 results enter the unchanged
sealed HarmonyCandidate at their requested positive cell height. Other results
explicitly report unsupported candidate index/charset/weight. The ordinary
candidate reports metrics, all256 CP1252 byte/glyph/raw-ABC rows, and a digest
of persistent, nonzero-pen RGBA canvases. Its copied input is overwritten/freed
before those operations. A successful candidate remains unqualified: metrics
and real-font coverage need comparison, and no provider capability is advertised.
In particular, no wrong TTC face is opened as face0 and no symbol cmap is guessed.

`--owned-registration authored.ttf output.tsv` accepts only the exact3240-byte
authored plain-geometry font from the sealed raster fixture (SHA256
069885a3e451df7439b6ae8279e942b22bf4017468e4868dd94ae936bdf2659a).
It first refuses an already visible fixture family. Using the actual shared
registration target, it acquires the same file twice for one stable owner and
once for a second owner, verifies visibility, releases the first owner and
checks the peer still sees it, then releases the second and checks absence.
Only process scope is used. Failure stays visible. Stable owner-token storage
and the strong-linked service persist until process exit; no failed-release
module unload is attempted or claimed safe. The input is authored, not an OS
font program, and its bytes are never installed persistently.

Exit0 means the requested measurement completed, including ordinary unavailable
rows; it does not mean all required fonts/features exist. Structural/identity,
owned registration, pixel/canvas or IO failure returns1; bad CLI/output returns2.
Both modes use synchronous calls only, no windows/event pumping, preferences,
clipboard, device, network, game, renderer or production owner factory.

`prepare_macos.py` freezes sources, compiles Release/Debug/sanitizer ARM64/min14
images, emits proposed future commands with45-second fixture watchdogs, and
records identities/imports/dependency hashes. It intentionally never launches
the produced images. The dependency prefix must be the pinned source-built
macOS14 FreeType2.14.3 prefix. Raster uses its existing strict-FP/NoEH policy;
discovery/probe are C++17/NoEH; registration retains exceptions for its existing
catch boundary. No filter/build-option changes are made.
