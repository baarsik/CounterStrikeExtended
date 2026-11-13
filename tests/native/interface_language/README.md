# Interface language startup selection

```sh
python3 tests/native/interface_language/run.py \
  --root . --evidence build/interface-language-green
```

The fixture compiles both complete production filesystem selection consumers,
the surface's production initialization statements and complete `SetLanguage`,
and the actual native/Windows dedicated language initialization statements.
Unrelated Steam/filesystem services are controlled; no game directories, Steam
preferences or registry values are changed by this consumer fixture. Extraction checks that dedicated
selection precedes the first catalogue load. ASan/UBSan cover ROOT, recursive
liblist/FALLBACK and UI agreement, Russian/Polish, explicit English, case
normalization, a custom identifier, missing values, next options, paths,
traversal, punctuation, whitespace, Unicode identifiers, 31/32-byte boundaries,
nonterminated surface input, unavailable Steam/system services and an oversized
dedicated command-reader result. Saved Russian/Polish preferences, automatic/reset,
invalid and unbundled saved identifiers, and explicit/custom command-line precedence
are checked across the same consumers. A source hash manifest accompanies extraction.

The production override is `-language russian` or `-language polish`. Any nonempty
ASCII identifier of at most 31 bytes containing only letters, digits and `_` is
accepted and normalized to lowercase. Invalid or missing overrides retain the
bundled saved preference, then the validated current Steam/surface language,
falling back to English when needed. Empty or invalid saved values select automatic.
The dedicated readers use their existing ISystem service for bounded 128-byte
command-line reads. Both the engine and dedicated readers use the shared native
preference reader, preserving the same registry type/length validation. The engine
reads it before filesystem paths are selected; that adapter
has a separate [native preference fixture](../interface_preferences/README.md).

The original command-line fixture and the saved-preference extension each failed
the ROOT search-path assertion before their production edits; the latter RED
source/binary evidence is in `build/interface-options-language-red`.
The green fixture validates consumers and selection, not full Windows process
startup or rendered game UI. Owning macOS `hw_engine`, `swds` and `hlds` builds
are separate checks; Windows x86 runtime validation remains separate.
