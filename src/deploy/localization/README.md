# Bundled interface translations

The maintained UTF-8 sources in this directory cover the stock Counter-Strike 1.6
and Half-Life interface, dedicated administration, and original CSE additions.
The build generates compatible runtime encodings and stages the complete pack.
Steam and game installations are not needed during ordinary builds.

## Languages and ownership

Both games advertise English, French, German, Italian, Spanish (Spain), Simplified
Chinese, Traditional Chinese and Korean. `manifest.json` records their Steam API
identifiers and the legacy `korean` alias for `koreana`. Russian and Polish are
additional CSE-maintained languages; `official_languages` distinguishes the
original eight from these extensions.

The eight stock token families are `valve`, `cstrike`, `gameui`, `platform`, `vgui`,
`serverbrowser`, `admin` and `server`. Original CSE text is kept separately in
`platform/resource/cse_<language>.txt`, including the inherited administration
console's `Console_Submit` token. Stock translation text remains Valve material;
keeping it here does not relicense it under the project's root license. Preserve
upstream notices and attribution when updating or distributing the files.

Select the interface language in **Options → Interface → Language**. The tab follows
Video and lists all ten bundled languages by their native names. Apply or OK saves
the CSE preference and forces a game restart after applying all Options pages;
Cancel discards the pending selection.
“Use Steam language” clears that preference. A failed save leaves Options open
with a retryable error and does not restart. Unchanged selections do not restart.

An explicit launch option such as `-language russian` or `-language polish`
(for example, `hl.exe -game cstrike -language polish`) takes precedence over the
saved choice. The page displays a notice while this option is present. Without a
valid saved choice or launch override, the existing Steam language remains the
default. The same startup resolution controls localized game paths, client UI
and the dedicated GUI. No Steam preference is changed.

The CSE setting is stored under
`HKEY_CURRENT_USER\Software\CounterStrikeExtended\Interface\Language` on Windows.
macOS stores the same key in the existing `org.counterstrikeextended.settings`
CFPreferences domain. Startup readers reject malformed or unbundled saved ids.
The manifest's `languages` and `native_language_names` generate the dropdown table
and saved-id allowlist during the normal offline build.

## Provenance

The initial import was made on 2026-11-13 from official Steam content. Each
manifest entry records its app, depot, manifest ID, original relative path,
original byte hash and encoding. Local machine paths, account metadata and
authentication material are deliberately absent.

- CS language depots: app 10, depots 12, 13, 14, 15, 17, 18 and 19.
- HL language depots: app 70, depots 72, 73, 74, 75, 77, 78 and 79.
- Shared server-browser resources: app 70, depot 3.
- Administration and shared platform/VGUI resources: app 90, Windows depot 5.
- English game catalogues and legacy text: the installed official Steam tree,
  with per-file hashes and owning content identities recorded in the manifest.

Russian and Polish reuse the official platform/admin/server-browser text that
exists in those depots. Missing game/catalogue/legacy text was authored from the
maintained English baseline. Authored entries identify that source and its hash;
fit adaptations and missing resource aliases are maintained CSE additions.

No community translation pack was imported. The maintained sources normalize
line endings to LF and use UTF-8; hashes describe the original upstream bytes,
before that conversion.

Valve's Simplified Chinese map-briefing files have damaged UTF-16 byte alignment.
Twenty-three fail strict decoding, and the remaining two contain the same damaged
newline pattern. The maintained versions of all twenty-five were instead derived
from the corresponding valid official Traditional Chinese files using ICU 79.1's
`Traditional-Simplified` conversion. Each entry records both the valid source and
the hash of the damaged file it replaces. Review the maintained Simplified Chinese
text directly when making subsequent wording changes.

## Fallbacks and legacy text

The loader reads English first and overlays the selected language. Some official
translations omit tokens that are present in current English catalogues. The
`upstream_fallbacks` section lists these omissions explicitly; they use the stock
English fallback. Every CSE token is present in all ten catalogues.

Legacy HUD messages, spectator menus, command menus, key-binding labels and map
briefings keep the game-relative paths used by the filesystem. The manifest
preserves their individual runtime encodings, including UTF-16 files supplied by
Valve. Language-specific resources are also exposed at the original localized
game paths. Missing upstream localized menu variants use the base game files.
The title-message, command-menu and CS client/server briefing readers decode BOM-marked UTF-16
using explicit 16-bit cells before parsing. UTF-8 BOMs are removed; files without
a BOM keep their original bytes. This supports both native 32-bit `wchar_t` and
Windows without treating the disk encoding as a native wide-character buffer.

Audio, maps/models themselves, README files and server-owned MOTD/configuration
are outside this text pack. Text briefings accompanying stock maps are included.

## Maintenance and verification

Edit the UTF-8 sources; do not edit generated output files. Add new CSE tokens to
every `cse_<language>.txt` file, preserving substitution arguments. The maintained
English catalogue also supplies the generated emergency browser fallbacks.

Keep game/product titles and recognized technology names in their canonical form:
Counter-Strike, Half-Life, Team Fortress Classic, Opposing Force, Condition Zero,
Steam, Valve, VAC / Valve Anti-Cheat, HLTV, OpenGL, Direct3D and similar brands.
Translate surrounding prose and common UI labels. Canonical VAC shorthand is valid;
generic words such as security, multiplayer and opposing forces in military prose
remain localized. This policy intentionally amends some historical Valve text;
`files[].cse_amendments` records the affected tokens while original upstream hashes
remain unchanged. The regression policy is `tests/localization_resources/brand_names.json`.

When refreshing stock text, acquire the exact desired official Steam depot into
a separate content cache with `download_depot APP DEPOT MANIFEST`. Import only
interface text, update the original byte hashes and content identities, and
recompute the explicit upstream fallback list. Do not copy account/configuration
files from a Steam installation into this directory.

Run:

```sh
python3 -m unittest discover -s tests/localization_resources -v
python3 tools/build_localization.py --source src/deploy/localization --output /absolute/test-runtime
```

CMake owns normal generation and staging. Its shared resource target runs on
subsequent builds as well, restoring deleted resource files without relinking
consumers or rewriting unchanged files.

Native consumer fixtures and the optional real VGUI loader check are documented
in [tests/native/README.md](../../../tests/native/README.md). These checks cover
resource loading and parsed text. Russian/Polish fit checks also use actual
scheme fonts, retained control layouts and native rendered samples. Full-game
interaction and all-language CJK acceptance remain separate checks.
