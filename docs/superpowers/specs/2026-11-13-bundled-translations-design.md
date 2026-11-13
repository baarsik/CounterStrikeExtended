# Bundled CS/HL translations

## Intent and agreed scope

CounterStrikeExtended will maintain and bundle text translations for the entire
stock Counter-Strike 1.6 and Half-Life interface, together with its own additions
and the dedicated-server administration UI. The user selected maintaining Valve's
existing translations as the baseline, with CSE additions alongside them.

Both games currently advertise the same eight interface languages:

| Language | Steam API/file identifier |
| --- | --- |
| English | `english` |
| French | `french` |
| German | `german` |
| Italian | `italian` |
| Spanish, Spain | `spanish` |
| Simplified Chinese | `schinese` |
| Traditional Chinese | `tchinese` |
| Korean | `koreana` |

Sources, checked 2026-11-13:

- https://store.steampowered.com/app/10/CounterStrike/?l=english
- https://store.steampowered.com/app/70/HalfLife/?l=english
- https://partner.steamgames.com/doc/store/localization/languages

The legacy `korean` filename spelling must remain compatible where the existing
game content or a consumer uses it. It is an alias, not a ninth language.

## Current behavior and evidence

- Game resources are supplied independently of compiled runtime outputs.
- The VGUI localization table reads UTF-16 token files with a BOM. For a
  `%language%` filename it loads English first, then overlays the selected language.
- The client gets its language from Steam. Its filesystem also searches
  `<game>_<language>` directories before the base game directory.
- The stock token families used by the retained runtime are `valve`, `cstrike`,
  `gameui`, `platform`, `vgui`, `serverbrowser`, and dedicated administration
  `server`. The installed English files contain roughly 2,200 key/value entries
  before adding the administration catalogue.
- Only `src/deploy/platform/Admin/server_english.txt` is currently versioned.
  It is inherited from the initial SDK import.
- Fourteen `CSEServerBrowser_*` English fallbacks are embedded in
  `src/GameUI/native/ServerBrowserDialog.cpp`.
- Older HUD message paths load `titles.txt`, independently of VGUI token files.
  Therefore token-file staging alone does not cover the entire stock interface.
- `cse_runtime_assets` already restores copied runtime assets on subsequent builds,
  including when the owning executable itself is up to date.

## Approaches considered

1. **Maintain Valve translations and separate CSE additions — selected.** Import
   stock catalogues with provenance, preserve their tokens and text, and keep
   project additions in their own catalogue. This preserves familiar wording and
   makes project-specific coverage visible.
2. **Independently rewrite all translations.** This would require translating and
   reviewing the entire stock catalogue again and would lose the established
   terminology. The user did not select this approach.

## Catalogue ownership and import

Keep the maintained source catalogue under `src/deploy/localization/`. Use UTF-8
text for reviewable source changes. Record the language set, catalogue families,
runtime destinations, input hashes, source provenance and any upstream fallback
gaps in a machine-readable manifest. Record Valve attribution alongside the
imported files; do not label imported translations as original CSE material.

Use the installed official Steam content as the initial baseline. Obtain missing
localized resources from official Steam content into a separate acquisition cache
without changing the user's game installation or language preference. Do not use
unattributed community packs or translations from a different game as a stock
baseline. Pin the resulting imported inputs so ordinary builds are offline and
independent of a developer's Steam installation.

Import all tokens in each stock family, including stock tokens unused by the
currently selected game. Add proper translations of CSE-specific strings and of
administration strings missing from the acquired baseline for all eight languages.
Preserve upstream English fallback entries as identifiable upstream gaps, rather
than claiming that copied English is a new translation. Missing upstream files
must be explicitly reported and resolved before claiming the requested pack is
complete.

## Generated runtime files and staging

Use a deterministic build-time generator to convert maintained UTF-8 token files
into the UTF-16LE-with-BOM representation required by the existing VGUI loader.
Preserve control sequences, placeholders, accelerator markers and token spelling.
Generate into the build tree, not back into `src/`.

Preserve the stock runtime paths and filenames:

- `valve/resource/valve_<language>.txt`
- `valve/resource/gameui_<language>.txt`
- `cstrike/resource/cstrike_<language>.txt`
- `platform/resource/platform_<language>.txt`
- `platform/resource/vgui_<language>.txt`
- `platform/servers/serverbrowser_<language>.txt`
- `platform/Admin/server_<language>.txt`

Add a separately named CSE catalogue, such as
`platform/resource/cse_<language>.txt`, loaded through the existing VGUI service.
This keeps its token namespace separate without introducing another localization
engine. Prefer the existing English-first fallback behavior.

Stage official localized legacy message files in the corresponding
`valve_<language>` and `cstrike_<language>` directories, preserving their expected
format and encoding. Inventory accompanying interface text, including menu and
key-binding label files, and include their official localized equivalents where
applicable. Preserve the behavior of the original legacy rendering path; this
change does not introduce a new HUD renderer.

Use one shared CMake-owned generation/staging dependency, avoiding concurrent
writers when several runtime targets are built together. Attach it to all relevant
consumers so full runtime builds, client builds, dedicated builds and applicable
engine-only builds receive the resources they need. Cover Windows x86, Windows
x64 and macOS ARM64 in Release and Debug, including custom output roots. Missing
staged files must be restored by the next build even if no binary is relinked.

Keep resource inventory information visible to the existing CMake inventory query
tools. Update the old administration staging list and its checks to avoid duplicate
ownership of `server_english.txt`.

## Loading behavior

The stock loader calls continue to use their existing paths. Load the CSE catalogue
before its strings are first used. The normal bundled path must obtain English
and translated CSE text from resource files. Retain an emergency missing-file
fallback only if required by existing component or runtime contracts, and keep it
synchronized with the maintained English catalogue rather than creating a second
independently edited translation source.

Preserve the existing ability to supply custom resources through filesystem search
paths. Check overlay precedence so bundled resources work without accidentally
discarding a selected language or loading English after its translated counterpart.
Do not add a language picker or change Steam preferences in this task.

## Validation and acceptance

- Every advertised language has the required stock families and CSE/admin text.
- The manifest distinguishes stock baseline coverage, upstream fallbacks, and
  complete project-owned token coverage.
- Token files parse successfully, preserve non-ASCII characters, have no accidental
  duplicate project tokens, and preserve substitution arguments and accelerators.
- Generated files load through the actual localization implementation, including
  a Western language and the Chinese/Korean paths. English fallback works when a
  translation entry or language file is absent.
- CSE strings resolve to translated values rather than embedded English in the
  ordinary bundled configuration.
- CMake dependency and inventory checks cover supported platforms/configurations;
  actual host builds demonstrate generation, staging, update propagation and
  restoration after deletion without relinking.
- Existing localization and affected server-browser/admin checks continue to pass.
- A small rendered UI sample checks longer Western labels and CJK glyph rendering
  if the existing local runtime can be exercised. Any unavailable Windows or
  whole-game verification is reported precisely.

## Boundaries and implementation discretion

This covers interface text. It does not bundle audio, maps, models, fonts or other
game assets, translate console/developer diagnostics, or localize arbitrary mod,
server or player-provided text. No new languages beyond the official eight are
required, apart from preserving the Korean alias.

The builder may choose the exact source subdirectory structure, manifest schema,
generator invocation and focused test integration. Reuse existing CMake policies
and localization code. New and edited C++ follows repository `AGENTS.md`.
