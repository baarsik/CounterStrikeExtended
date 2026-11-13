# Team-menu map description cache

Run on macOS with the C++ compiler and ASan/UBSan:

```sh
python3 tests/native/team_menu/test_map_page.py --root . --evidence build/team-map-page
```

The fixture includes the production CTeamMenu declaration and extracts its page loading,
Update, Activate and invalidation bodies, plus the viewport's OnLevelChange body. Only
filesystem and UI services are controlled. It records source hashes, compiler arguments,
and each process result; it does not run a real VGUI window or measure gameplay performance.

The cases cover repeated successful score updates without redundant file reads or scroll
resets, preserved label/team-button work, empty files, requested-map fallback identity,
missing/Open/short-read retries, map changes, same-map level reset, and immediate refresh
when opening the menu. Failed activation refreshes revoke the previous cache entry.
Successful descriptions remain cached between menu openings, map changes and level resets;
this is not continuous file hot reload.

The Unicode case feeds every maintained stock map briefing in its declared disk
encoding into the actual `LoadMapPage` method and compares the complete UTF-8
text handed to the UI. Additional little-/big-endian UTF-16 inputs include Western,
CJK and supplementary characters. This checks decoding on macOS's four-byte
`wchar_t`; it does not qualify rendered fonts or layout.
