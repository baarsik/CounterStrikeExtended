# Legacy localized resource consumers

Run on macOS with Clang and ASan/UBSan:

```sh
python3 tests/native/localization_text/test_messages.py --root . --evidence build/localized-titles
python3 tests/native/localization_text/test_command_menu.py --root . --evidence build/localized-spectator-menu
python3 tests/native/team_menu/test_map_page.py --root . --evidence build/localized-map-pages
```

Use a new evidence directory for each run. The title fixture extracts the actual
engine initializer, parser, directives and line readers, with controlled file and
allocation services. It checks every message name and complete body in all 16
bundled CS/HL title files, using each file's declared runtime encoding. The map
fixture uses the actual client page methods with controlled filesystem/UI services.
The command-menu fixture combines the production combo-menu method with the
engine's actual byte tokenizer and checks all six Korean spectator labels and
file-allocation ownership.

The fixtures compare parsed text, not glyphs or game windows. They preserve the
baseline byte encoding for files without a BOM and test UTF-16 decoding before
the byte-oriented parser. The portable CMake decoder test and the actual VGUI
localization loader checks are described in [the native test reference](../README.md).
