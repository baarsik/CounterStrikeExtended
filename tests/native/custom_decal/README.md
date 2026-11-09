# Native custom-decal component

The fixture runs complete retained loader, custom-cache, validation and HW/SW CPU texture-builder bodies, and
links the real `com_custom.cpp`. It uses real SDK declarations with controlled filesystem, heap/cache, HPAK
and renderer services. It does not initialize a renderer, upload through a graphics provider or run a game.

```sh
cmake -S tests/native/custom_decal -B build/custom-decal-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/custom-decal-release --parallel 2
ctest --test-dir build/custom-decal-release --output-on-failure
cmake -S tests/native/custom_decal -B build/custom-decal-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/custom-decal-debug --parallel 2
ctest --test-dir build/custom-decal-debug --output-on-failure
cmake -S tests/native/custom_decal -B build/custom-decal-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_CUSTOM_SANITIZERS=ON
cmake --build build/custom-decal-san --parallel 2
ctest --test-dir build/custom-decal-san --output-on-failure
```

Use a matching MSVC environment on Windows. `CSE_CUSTOM_SDK` selects actual headers;
`CSE_CUSTOM_BODY_SDK` can select the frozen pre-change complete source bodies for reference builders. The
independent Python WAD writer uses fixed little-endian 12/32/40-byte records, rather than C++ `sizeof` values.
The pure record helper compiles as C++14; the surrounding engine-body fixture uses the SDK's C++17 policy.
Mac Release uses explicit `-Os`, Debug `-O0 -g`, with NoEH, unwind tables and precise noncontracted FP.
MSVC uses `/W4 /WX /fp:precise /EHsc`, with `/O2` or `/Od`. Actual production owning commands are a
separate compile gate, retaining their own per-file policies.

The 24 native cases cover exact supported CPU bytes; padded mip offsets; odd and 1x1 dimensions; all palette
counts 0..256; ordinary texture use of the larger cache prefix; validation/cache miss/hit/eviction/reload;
allocation rollback at all 11 creation stages; corrupted copied-cache cleanup and retry; short file reads;
HPAK actual-length admission; initialized backing tails; loader callback path/mode/length rewrites;
IGNOREINIT/WIPEDATA behavior and the initialization-only 20 KiB limit. The parser also places every truncated
WAD/mip prefix directly against an inaccessible owned page, with atomic output and high-address controls.
Win32 runs 20 cases because its native-only malformed/failure repairs are intentionally unselected.

The private customization loader calls the original hook once, initially with a null length pointer, and
maintains a separate completed byte count that the hook cannot rewrite. A hook-selected nonheap mode is
refused before opening or allocating: customization destruction owns `Mem_Free`, so another allocator's
result cannot safely enter that path. The ordinary public loader retains other-mode hook behavior. The
native heap read accepts only a representable allocation and a complete byte result. This does not make
all retained filesystem modes or arbitrary callback pointers safe.

The checked records bound both retained palette-count addressing (`40 + sum(area >> shift)`) and the
renderer palette (`offset0 + sum + 2`, all 768 bytes). Odd dimensions retain the original area shifts.
The native cache prefix derives from the actual texture/mip layouts: HW 56 bytes, SW 40 bytes on the tested
64-bit targets, versus original Win32 32/24. No mip payload may overlap the expanded native header.

One explicit malformed-input compatibility exception is retained in the evidence: the original Win32
builders accept a physically contained `offset0 < 40` header/pixel alias, then overwrite some pixel bytes
with representation-dependent texture fields. A safe real Win32 witness was frozen before the native
rule was added. Native admission rejects that class. Safe noncanonical offsets at or above 40, gaps,
odd dimensions and palette counts remain supported. No malformed pointer/header bytes are emulated.

`check_legacy_tokens.py` verifies all five edited existing files against frozen Win32 selected tokens,
including the original Windows SEH body. `run_negative_controls.py` makes isolated imported-body mutations:
retaining an invalid cache entry, admitting a nonheap hook result and restoring the undersized HW prefix
must each fail a behavioral assertion. It never edits production or the retained reference.

The qualification packet lives under the dated artifact root, `custom-decal-v1/`. It preserves pre-change
short-read/prefix/palette-span failures, actual Win32 builder images and overlap witness, all six Windows
CPU/config receipts and machine/hash identities, Mac R/D/sanitizers, cross-profile exact bitmap/palette
comparisons, full owning-TU compile commands/dependency hashes, and the visibility-only final-header
supplement. Frozen reference v1 and the first current Windows fixture compile failure remain separate.

Limits: the HPAK service is a typed actual-byte-contract seam, and cache allocation/eviction uses an owned
facade rather than the real hunk/LRU implementation. Whole `com_custom.cpp` and imported complete methods
are actual source; surrounding services are controlled, not ABI or provider acceptance. Genuine initialized
spans, SDK allocator ownership, little-endian retained targets and serialized customization/cache lifetime
are required. Nonreturning allocator/provider failures are not converted into recovered success. Full
client loading, spray rendering, network delivery and renderer/provider failure acceptance remain open.
