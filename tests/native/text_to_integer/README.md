This packet provides an unwired, value-only native equivalent of the retained
compiler-linked Win32 UCRT `atoi`, `_atoi64` and `_wtoi` calls. It changes no existing
headers, callers, build membership or Windows selection. Runtime integration must
keep the real CRT calls on Windows and select these helpers only at the native
compatibility boundary.

`CseLegacyAtoi(const char*, CseLegacyIntegerByteWhitespace)` returns exactly
`int32_t`; `CseLegacyAtoi64(const char*, CseLegacyIntegerByteWhitespace)` returns exactly
`int64_t`; the default `COrUtf8` profile skips ASCII whitespace only. The explicitly
selected `SingleByteNbsp` profile additionally skips raw byte A0, as measured for
Windows CP1252/CP1250. Input is bytes, not decoded UTF8. These are measured profiles,
not an emulation of arbitrary Windows code pages or the host's current locale.

`CseLegacyWtoi(const wchar_t*)` returns exactly `int32_t`. It consumes the measured
UTF16 code-unit domain despite native `wchar_t` width: the retained 26 whitespace
units and 18 BMP decimal digit blocks, ASCII sign, then decimal digits until the
first unrecognized unit. Surrogates, non-BMP native wchar values and negative
native wchar values stop parsing. The table deliberately retains U+180E and does
not adopt the host's evolving Unicode/locale classifications.

All three helpers require non-null NUL-terminated input, return zero when no digits
are accepted, ignore trailing text, recognize no hexadecimal/octal prefix, and
saturate to the signed result width on overflow. Unsigned magnitude/cutoff
arithmetic handles the exact negative minimum without signed overflow or an
out-of-range signed cast. They allocate nothing, retain no input, mutate no
locale/global state and preserve errno. CRT errno and invalid-parameter handler
side effects are intentionally outside this value-only contract; the actual
callers assign the returned value and do not inspect errno.

Caller evidence: engine `CL_UpdateUserinfo` and `CL_ConnectionlessPacket` consume
decimal Steam-ID text, then assign/cast the signed 64-bit result into uint64
storage. The parse therefore retains the signed-64 saturation even for uint64
destinations. `KeyValues::GetInt`, `KeyValues::GetUint64` and `TextEntry::RequestInfo`
consume wide text and retain signed-32 results before any later uint64 conversion.
The public VGUI KeyValues POSIX fallback currently uses `(int)wcstol`; root should
review that caller too when wiring. VGUI controls set the user-default LC_CTYPE;
therefore C-only whitespace was not assumed for the wide conversion.

The reference-first Windows probe calls the real linked CRT for x86/x64/ARM64
Release/Debug, records the actual function-owning DLL path, loaded PE machine,
file SHA256 and version, and compiler identity. Release resolves to ucrtbase.dll;
Debug resolves to ucrtbased.dll. It measures startup C, isolated per-thread user
CP1252, and explicit C/UTF8/CP1250 locale objects without changing a machine setting.
It separately measures legacy msvcrt.dll, whose wide overflow/whitespace values
differ and are not the approved target. The first probe's legacy errno column
used the linked CRT's errno slot; V2 and final V3 correctly query the legacy
provider's own `_errno`. This correction did not change linked-UCRT goldens.

Each profile contains 627 encoded corpus cases: decimal/sign/prefix/trailing text,
32/64-bit limits and overflow, 4096-digit inputs/leading zeroes, embedded NUL,
UTF8 byte sequences, Unicode digits/spaces, surrogates and position controls around
every observed range. Separate linked `atoi` telemetry measures the narrow corpus and both prefixes
for all 256 bytes (512 values), with its actual provider recorded too. Two full
`_atoi64`/`_wtoi` tables measure every byte and every BMP unit as a
prefix before `7` and `+7` (131584 exact encoded values). All six Windows compiler
profiles agree for each measured CRT/locale profile. The native helper tests use
the frozen x86 goldens and check five linked profiles, plus 2097152 positions
across all non-BMP Unicode scalars and four higher/negative native wchar controls.
The direct macOS LP64 `wcstol` control fails those same golden values; its failure
and the missing-helper compile RED are retained. No production CRT warning
suppression or external implementation was imported.

```sh
cmake -S tests/native/text_to_integer -B build/text-integer-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCSE_TEXT_BUILD_HELPER=ON \
  -DCSE_TEXT_REFERENCE=/absolute/path/to/frozen/windows/x86-Release
cmake --build build/text-integer-release
ctest --test-dir build/text-integer-release --output-on-failure -V
```

Use Debug for Debug; add `-DCSE_TEXT_SANITIZERS=ON` for ASan+UBSan. All helpers and
fixtures compile as strict C++14. Mac fixtures use warnings-as-errors including
conversion warnings and minimum macOS14. Windows fixtures use `/W4 /WX /EHsc`.
`windows_reference.py` validates a frozen source manifest, initializes each MSVC
environment only in RAM, builds with one job and records fresh real reference
calls; `--helper` adds the actual header comparison tests for all six CPU/config
profiles. It needs a new physical workspace and separate immutable receipts.

This is a scalar conversion component gate. It does not qualify engine/UI data
flow after wiring, all locales/code pages, null/malformed pointers, full native
modules or gameplay. There are no network, game, Steam, CEF or UI executions.

The original two-helper V1 packet is preserved before the separately approved
`atoi` extension. This packet does not implement strtol/strtoul end pointers or
scanning semantics. Those actual cd.cpp/utlbuffer.cpp consumers remain a separate
follow-up lane.
