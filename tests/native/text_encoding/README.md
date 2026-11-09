# Legacy VGUI2 UTF8 / UTF16 cell compatibility

Only `common/legacy_text_encoding.h` and the three native conversion branches in LocalizedStringTable.cpp change.
The Windows-selected bodies remain byte-identical, including CP_UTF8/flags0, `-1` source length and forced final
NUL writes. The non-Windows iconv include is replaced by the pure C++14 header. No new dependency, allocator,
provider, log, global state or locale mutation is introduced by production code.

## Contract established by actual Windows bodies

The fixture extracts all THREE retained function bodies verbatim and links them to real Windows APIs. The first
Win32 run, then x86/x64/ARM64 Release/Debug, produced the same 4732-row CSV in C and process-local `.UTF8` locales.
The compressed, exact CSV has SHA256 `14dbac63e7d3810e9b14a2517d475c9defd7417c0de32b045d58d371d311de2c` when decoded.
Only the test's CSV line ending is parsed; encoded output bytes and cells are compared without tolerances.

| Function | Source / output | Capacity and result |
|---|---|---|
| ConvertANSIToUnicode | UTF8 → UTF16 code units in wide cells | capacity is destination BYTES; result counts cells including NUL |
| ConvertUnicodeToANSI | UTF16 units → UTF8 | capacity is destination BYTES; result counts UTF8 bytes including NUL |
| Q_UCS2ToUTF8 | uint16 NUL-terminated units → UTF8 | same byte result; ALSO preclears byte0; cubSrcInBytes is ignored like Windows |

Native wchar_t32 output stores each UTF16 unit in its own cell. Supplementary characters therefore occupy two
cells, preserving Win indexing. Wide-to-UTF8 combines surrogate pairs. It additionally accepts valid native32
scalars as a convenience for existing native literals/writer inputs. Negative native32 values, values above
10FFFF and unpaired surrogates become U+FFFD. Embedded NUL ends the source, including Q regardless of sourceBytes.
Input allocations must be valid NUL-terminated strings; destination allocations must cover complete reported cells.

Success includes NUL in the return count. Insufficient capacity returns0 after retaining any complete converted
prefix, then the wrapper forces the final destination cell/byte to NUL. That final write may remove a low
surrogate or UTF8 byte; this measured legacy behavior is preserved rather than repaired silently. A successful
short output also leaves an extra NUL in the final destination slot while preserving intervening spare data.
Null source with valid destination returns0 and only forces the last slot (Q also forces first byte).

Old wrappers write before a logical zero-cell buffer or dereference null destination. Native equivalents choose
bounded0/no-write for null destination, negative/zero byte capacity, or capacity smaller than one wide cell.
The corpus separately labels625 such unsafe Windows-domain observations; they are not claimed as byte parity.
All remaining4107 observations compare return and every output/guard cell exactly.

Windows flags0 replacement differs from generic UTF8 repair: `E0 80 80` and `ED A0 80` each yield two U+FFFD;
a CESU8 surrogate pair yields four. The decoder consumes a continuation with an invalid second-byte range before
rejecting that prefix. It does not use Python replacement behavior or locale codecs as its production reference.
The main corpus includes BMP, surrogate pairs, isolated/repeated surrogates, noncharacters, BOM, nonBMP, embedded
NUL, every nonzero single byte, overlong sequences, CESU8, invalid/truncated sequences and capacity transitions.
A supplemental real-WinAPI comparison checks2560 UTF8 transition cases and ALL65536 single UTF16 units with both
16/32-bit cell helpers in every x86/x64/ARM64 Release/Debug configuration.

## Actual caller audit and boundaries

Q is called by AddFile on NUL-terminated `keytoken[128]` and `valuetoken[2048]`. Its sourceBytes argument is a consumed
source-file span or the entire array, not a reliable token extent; imposing it as a new bound would change Windows
behavior and can reproduce the old iconv overread. AddFile already widens each uint16 value token into a wchar_t
cell; the new representation agrees with that loader and the existing WriteLocalizationUTF16 writer.

Most selected callers pass sizeof(wide-buffer). Selected CS `SharedFindString` instead passes2048 for a1024-cell
array: that denotes1024 Windows cells but512 native wchar32 cells. This is a caller sizing issue outside this fixture;
the converter honors physical byte capacity. The unselected generic game_shared copy uses BufLen*2 similarly.
Selected common/CS ComboBox wide overloads allocate wchar count+1 UTF8 bytes, which can be too small for nonASCII
on Windows too. Their failure handling/allocation requires separate caller work; codec output is not weakened to
hide it. SpectatorGUI's deliberate null-source case initializes playerName to an empty wide string before calling.
No caller, Panel text/input, font, UI, clipboard, settings, Steam, CEF or network code is changed or executed.

## Fixtures

```sh
cmake -S tests/native/text_encoding -B build/text-encoding-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/text-encoding-release --parallel 2
ctest --test-dir build/text-encoding-release --verbose
cmake -S tests/native/text_encoding -B build/text-encoding-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/text-encoding-debug --parallel 2
ctest --test-dir build/text-encoding-debug --verbose
cmake -S tests/native/text_encoding -B build/text-encoding-san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_TEXT_ENCODING_SANITIZERS=ON
cmake --build build/text-encoding-san --parallel 2
ctest --test-dir build/text-encoding-san --verbose
```

Mac tests call the actual extracted native bodies, not reimplemented wrapper models. Windows tests compare both
cell widths directly with the pinned actual-body output; a separate executable runs the real API bodies. Locale
changes happen only in isolated fixtures. Guard tests use protected own pages and require addresses above4GiB on
64-bit targets; x86 runs the same boundaries without an impossible high-address requirement. Undefined zero/negative
capacity behavior is tested only against the new safe native policy. No V13 producer source or image is used.
The actual owning Mac LocalizedStringTable TU also compiles under retained Release/Debug commands and flags.

The RED receipt used actual old native bodies, with the old iconv link and one legacy sign-warning downgrade
required to execute that baseline. It reported4066 mismatches in4097 safe baseline observations. The candidate and
helper builds use strict warnings; all MSVC reference/helper builds are warning-free. Source, images, machine IDs,
raw records, compiler commands and source hashes are archived separately from runtime/target acceptance.
Current execution is macOS27.0.1 ARM64 with deployment target14, plus VM1 Windows APIs. These results do not qualify
actual macOS14, fonts/glyph display, complete localization, VGUI2 UI, all19 targets or ARM64 full-runtime delivery.
