# Bundled translations verification — 2026-11-13

The maintained pack contains 309 UTF-8 source files: 64 official VGUI catalogues,
eight CSE catalogues and 237 legacy interface files. Generation produces 371
runtime files including stock paths and Korean aliases. It covers English,
French, German, Italian, Spanish (Spain), Simplified Chinese, Traditional Chinese
and Korean. Ordinary builds use only checked-in sources.

## Provenance and inherited gaps

Stock text comes from authenticated official Steam depot downloads and the
installed official English content. The manifest pins app/depot/manifest identities
and original byte hashes. Twenty-five damaged Simplified Chinese briefings were
derived from the matching valid official Traditional Chinese files using ICU 79.1
Traditional-Simplified conversion; original and replaced hashes are retained.

Official VGUI catalogues omit 301 English token/language instances. These are
recorded as upstream English fallbacks, not counted as CSE translations. All 15
CSE/admin additions are present in every supported language. Other upstream text
and legacy encoding choices are preserved.

## Observed checks

| Check | Result |
| --- | --- |
| Python catalogue/generator/CMake fixtures | 13 tests pass; Unicode, arguments, aliases, invalid input, updates and restoration covered |
| Windows x86/x64 and macOS ARM64 resource inventories, Release/Debug | All six expose the same 371 unique destinations |
| Native macOS Release production builds | GameUI, HLDS, hardware engine, HL/CS clients and CS server compile/link |
| VM1 MSVC x86 Release | Portable generation/staging suite and C++ disk-text decoder pass |
| Actual macOS VGUI/filesystem loader | Eight stock catalogue families plus CSE resolve all eight languages, Korean alias and English fallback |
| Actual title initializer/parser under ASan/UBSan | All 3,988 messages in 16 bundled files match names and complete text |
| Actual client team-menu reader under ASan/UBSan | All 200 bundled map briefings match complete decoded UI text; cache/retry cases pass |
| Actual combo-menu reader and engine tokenizer under ASan/UBSan | All six Korean spectator labels and file ownership pass |
| Actual CS world-spawn briefing path under ASan/UBSan | Seven translated languages pass both primary and default-file paths |
| Dedicated runtime resource inventory | 386 selected resources match expected staged bytes (371 translations plus 15 retained admin assets) |
| Browser component | 59 objects compile/link; localization, tabs and filters cases pass |
| Production resource restoration | Deleted French CSE file is restored byte-for-byte without relinking the current GameUI library |

## Review findings and limits

A fresh whole-branch review identified UTF-16 reaching byte-oriented title/menu
parsers and a CS briefing reader interpreting disk UTF-16 as native `wchar_t`.
Consumer tests reproduced both failures. Explicit 16-bit decoding now supplies
UTF-8 before parsing, independent of the platform's `wchar_t` size. Auditing the
same inputs also found and repaired the CS server briefing copy. Existing byte
encodings and the server's briefing-size limit remain intact.

The browser's existing `unsaved` case fails with “failed settings save disabled
otherwise usable browser.” Compiling and relinking the unchanged baseline
`ServerBrowserDialog.cpp` reproduces the same assertion. It is not claimed as a
passing test. The component also reports existing missing-Marlett font warnings.

Full-game interaction, rendered Western/CJK layout and legacy HUD CJK glyphs have
not been qualified. Windows game binaries were not built or run for this task;
Windows evidence covers the resource pipeline and the x86 decoder. The pack does
not introduce a new HUD renderer or supply fonts and other game assets.

Run commands and fixture boundaries are in [the native test reference](../../../tests/native/README.md)
and [catalogue maintenance notes](../../../src/deploy/localization/README.md).
