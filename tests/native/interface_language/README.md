# Interface language command-line selection

```sh
python3 tests/native/interface_language/run.py \
  --root . --evidence build/interface-language-green
```

The fixture compiles both complete production filesystem selection consumers,
the surface's production initialization statements and complete `SetLanguage`,
and the actual native/Windows dedicated language initialization statements.
Unrelated Steam/filesystem services are controlled; no game directories, Steam
preferences or registry values are changed. Extraction checks that dedicated
selection precedes the first catalogue load. ASan/UBSan cover ROOT, recursive
liblist/FALLBACK and UI agreement, Russian/Polish, explicit English, case
normalization, a custom identifier, missing values, next options, paths,
traversal, punctuation, whitespace, Unicode identifiers, 31/32-byte boundaries,
nonterminated surface input, unavailable Steam/system services and an oversized
dedicated command-reader result. A source hash manifest accompanies extraction.

The production override is `-language russian` or `-language polish`. Any nonempty
ASCII identifier of at most 31 bytes containing only letters, digits and `_` is
accepted and normalized to lowercase. Invalid or missing overrides retain the
validated current Steam/surface language, falling back to English when needed.
The dedicated readers use their existing ISystem service and a bounded 128-byte
buffer before the same validator. No language picker or preference write is added.

The initial fixture failed the ROOT search-path assertion before production edits;
its extracted source/binary evidence remains in `build/interface-language-red`.
The green fixture validates consumers and selection, not full Windows process
startup or rendered game UI. Owning macOS `hw_engine`, `swds` and `hlds` builds
are separate checks; Windows x86 runtime validation remains separate.
