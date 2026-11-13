# Russian and Polish bundled translations

Extend the approved bundled-translations design with `russian` and `polish`, covering the same entire stock CS/HL interface, administration and CSE additions. The user explicitly requested both languages and validation that translated/adapted text fits. Reuse the current manifest, offline generator, stock runtime paths and build dependencies.

## Approach

Continue the existing catalogue design. Reuse authenticated official Russian platform/admin and Russian/Polish server-browser text where present, author missing text from the maintained English sources, and identify authored/adapted material honestly. New catalogues cover every current English token; legacy files include titles, menus, key bindings and all map briefings. Commands, token identifiers, format arguments, names and key assignments retain their meaning.

An external translation service would introduce an unnecessary dependency and send source text elsewhere; partial CSE-only coverage would contradict the established whole-interface scope. Neither is needed. This is an extension of the already approved architecture; no new language selector or Steam preference change is required.

## Fit and acceptance

Validate with actual VGUI localization, font metrics and control layouts, using the retained schemes. Check Russian Cyrillic and Polish diacritics, minimum supported window sizes, button insets, list columns, long status text and wrapped descriptions. Adapt wording first; change shared layout only when text cannot remain clear at existing sizes. Produce rendered samples for visual inspection and a machine-readable fit report, and keep repeatable checks in the native fixtures. Distinguish tested sample coverage from whole-game acceptance.

Preserve the original eight languages and Korean alias. Maintain complete new-language token/argument coverage, generated file restoration and build independence from Steam. No game assets or fonts are added without an observed need. Keep work on the existing isolated branch; no push or merge is requested.

## Observed implementation requirements

Validation exposed byte-only Half-Life HUD/VGUI1 paths that could not render these
languages. The implementation preserves the public ABI and existing byte/TGA
glyphs while adding valid BMP UTF-8 iteration, native-font glyph fallback and
engine-owned Unicode measurement. Engine printable-character filtering no longer
depends on the C locale. A shared validated `-language` override selects new
resources consistently across game/fallback paths and client/dedicated UI without
changing Steam preferences.

Real Windows font checks also exposed desktop scaling in the initial Mac fixture.
The final fixture injects the existing engine geometry implementation at 640x480
and forwards all font/raster calls to the actual provider. Both native layouts
and Win32 x86 GDI font budgets must pass, including oversized negative controls.
