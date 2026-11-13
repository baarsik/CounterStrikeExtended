# Russian and Polish translation verification — 2026-11-13

Russian and Polish extend the entire maintained stock CS/HL, dedicated admin and
CSE interface. The pack now has 395 maintained UTF-8 files and 463 runtime destinations,
including all original languages and Korean aliases. Each new language covers the
complete current English token/argument baseline and all 34 English legacy files,
including 25 map briefings. Twelve missing legacy resource aliases are supplied in
each new language. Five CSE admin tokens replace 15 previously literal English
layout labels; every CSE token exists in all ten languages.

Use `-language russian` or `-language polish` at startup. One validated selector
controls game/fallback search paths and client/dedicated UI; no Steam/registry
preference changes or new picker are involved. Stock official text is reused
where available. Authored/adapted files retain English-source or official-depot
provenance in the manifest. Proper names, weapon identifiers, commands and key
assignments are deliberately preserved; compact buy labels retain identifying
models while descriptions preserve the fuller wording.

## Observed checks

| Check | Result |
| --- | --- |
| Catalogue/generator/CMake suite | 14 tests pass, including complete new-language keys, arguments and legacy destinations |
| Actual VGUI loader | All ten languages, stock/CSE families, Korean alias and English fallback pass |
| Actual retained controls at 640×480 game geometry | 665 checks pass per language, including 617 distinct resource constraints, minimum 640×384 browser, expanded filters and complete tab-strip edges |
| Font coverage | 156 Russian and 107 Polish alphabet characters resolve through actual Tracker/Client face/fallback selections |
| Native fit negative control | Deliberately overlong button is rejected |
| Windows x86 GDI on VM1 | 617/617 controls per language fit using eight actual Tahoma/Verdana configurations; 12 alphabet checks pass, 2 overlong negatives detected, 0 missing glyphs |
| HL spectator/command menus | All 30 current bound-key-prefixed labels fit actual native Arial 14 and retained default bitmap font with native Unicode fallback |
| HL menu safety regression | Actual localization, menu scratch/error copies and private button display pass ASan/UBSan without truncation; original binary still reproduces Russian overflow/Polish truncation |
| VGUI1 Unicode and ownership | 17 ordinary regressions and 4 focused sanitizer checks pass; legacy TGA/CP1252 glyphs, native fallback, atlas reuse/retirement and Win32 A/W choice covered |
| HL title scanning | 20 ASan/UBSan cases pass using actual scaled/legacy engine credits-font metrics |
| Engine draw gate | C-locale Cyrillic/Polish painting regression passes; shared catalogue text is no longer modified while filtering |
| Language selection | Actual ROOT/FALLBACK/client/dedicated consumer tests pass under ASan/UBSan |
| Native production | GameUI, HLDS, hardware/software engine, HL/CS clients and CS server compile/link |
| Dedicated resource inventory | 478 selected resources match staged source/generated hashes |

The actual title parser and map-page consumer also pass against the complete
expanded maintained pack. Runtime-source regeneration leaves no derived changes.

## Fit corrections and review

The initial standalone Mac surface used the host desktop height and enlarged
proportional CS budgets by 1.6. Independent Windows 480p metrics exposed that test
setup error. The final native fixture injects the existing engine geometry methods
at 640×480 and forwards all font/raster operations to actual providers. Labels and
paragraphs were then adapted against both font implementations; character counts
were never accepted as final fit evidence. Three admin caption boxes were enlarged
within their existing layout space after real-font clipping was observed.
The active browser page is explicitly selected before expanding filters. Its
unbounded game/map/filter preview retains the existing intentional ellipsis;
that policy is reported separately and does not excuse clipped translated
buttons, labels or expanded filter controls.

Validation also found byte-only HL HUD and VGUI1 paths. Valid BMP UTF-8 now reaches
Unicode glyph/metric operations without changing public engine/VGUI interface slots
or public VGUI object layouts. Existing byte fonts/TGA cells and malformed/CP1252
runs retain their legacy behavior. Bitmap and FixedSys fonts use existing native
families for missing Unicode glyphs. Private Unicode Latin-1 identity prevents
UTF-8 `ó` from being misread as ANSI byte 0xf3 on a Cyrillic Windows code page.

The fresh review found one important issue: Russian spectator labels exceeded a
32-byte HL menu buffer through an unbounded title substitution. The regression
failed under ASan before repair. Bounded UTF-8-safe expansion, matched menu scratch
capacity and dynamic private button storage/display now preserve full labels.
Actual font measurement found one remaining long label; «Фикс. камера слежения»
fits 153px in its 196px available native-font area and 146px with the bitmap font.
The review's indentation finding was corrected. No material finding remains open.

## Evidence and limits

Repeatable commands are in [resource/fit tests](../../../tests/localization_resources/README.md),
[HL menu tests](../../../tests/native/hl_menu_localization/README.md),
[HL HUD tests](../../../tests/native/hl_message_utf8/README.md),
[VGUI1 tests](../../../tests/native/vgui1_utf8/README.md) and
[language selection](../../../tests/native/interface_language/README.md).
Native fit reports and actual rendered PNG samples are generated by `run_fit.py`;
this run used `build/russian-polish-fit/delivery-native`. Windows results are in
`build/russian-polish-fit/final-windows`.

These checks establish the specified resource/control/font inputs. Full-game
interaction, all dynamic server/player substitutions, complete Win32 VGUI1 raster
execution, Linux runtime rendering, supplementary Unicode/grapheme shaping and
exhaustive human linguistic review remain unqualified. The existing browser
`unsaved` settings case was already reproduced with the baseline implementation;
it is unchanged and is not claimed as passing. No push or merge was performed.
