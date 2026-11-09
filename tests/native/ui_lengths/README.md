These fixtures compile the actual changed production HUD bodies, CUtlBuffer methods,
and localization save/conversion methods. Only their game, surface, localizer, and
filesystem dependencies are controlled. Extraction receipts record source and body
SHA256 values. The component does not initialize the renderer, game, audio, or UI.

The original x64 C4267 sites have the following count proofs:

| Source operation | Count/storage proof |
| --- | --- |
| `COM_FileBase` | Input indices, extension search, and copy length stay `size_t`; exclusive bounds also cover empty paths and paths without a directory separator. The existing caller-owned output-capacity precondition remains. |
| Spectator player name | `snprintf` terminates the 256-byte drawing buffer. Its length is at most 255 before conversion to an int and multiplication by the existing three-pixel half-width (maximum 765). |
| HUD font text iteration | `wcslen` and iteration stay `size_t`; the font API continues receiving one existing `wchar_t` at a time. Existing int pixel-width accumulation is unchanged. |
| Chat CR/LF trim | Length stays `size_t`; trimming precedes copying at most 511 bytes into the 512-byte terminated buffer. Previously a long line ending with CR/LF could index past that buffer. |
| Chat temporary and translation | The temporary wide text is terminated inside 256 elements. Remaining counts and arbitrary translation lengths stay `size_t`; copies clamp to the remaining 256-element output. Token copies independently clamp to the 128-element word buffer. |
| Console partial text | `strlen` stays `size_t`. The 256-byte member is filled through `TextEntry::GetText`/the terminating localization conversion. No narrowing is needed for its final-space test. |
| `CUtlBuffer::PutString` | The string plus optional binary terminator must fit `INT_MAX - m_Put` before conversion. `CheckPut` also rejects negative counts/cursors and signed cursor overflow, and limits the existing capacity-growth arithmetic. Binary `GetString` finds its terminator inside the int-sized allocation before converting the pointer difference. |
| Localization values | The arbitrary string length is never converted to int. UTF-16LE output streams in 1024-byte chunks, so every filesystem write count fits int. |

The localized resource envelope remains the Windows format: `FF FE`, UTF-16LE
`lang`/`Language`/`Tokens` text, the same quote/tab/CRLF punctuation, and the legacy
terminating UTF-16 zero after the final braces. Existing UTF-16 surrogate units are
preserved verbatim; a full scalar in a 32-bit `wchar_t` is encoded as a surrogate pair.
Invalid values above Unicode's scalar range fail the save. Short writes return false
and close the file. Quote/escape policy is unchanged.

The exact-byte fixture feeds the generated units through the existing production
`ReadUnicodeToken` implementation and confirms BMP text is understood. This is a
bounded loader-token check, not qualification of all `AddFile` behavior, fonts,
supplementary-character rendering, or the complete localization service. The current
loader widens UTF-16 units individually; no general Unicode/font rewrite is included.

The original ten cases cover basename bytes/canaries, font iteration, bounded spectator names,
large chat lines/translations/tokens, binary/text buffer envelopes and cursor limits,
UTF-16LE bytes and surrogate/scalar equivalence, streaming large values, and failed
open/short writes. Release, Debug, and ASan+UBSan run the same cases. Windows fixture
targets use production's `_CRT_SECURE_NO_WARNINGS` policy while retaining `/W4 /WX`.

Run from the repository root:

```sh
cmake -S tests/native/ui_lengths -B build/ui-lengths-macos-arm64-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui-lengths-macos-arm64-release
ctest --test-dir build/ui-lengths-macos-arm64-release --output-on-failure
```

Use a separate build directory and `Debug`; add `-DCSE_UI_LENGTH_SANITIZERS=ON` for
ASan+UBSan. For Windows initialize the matching MSVC environment, pass
`-DCSE_TARGET_ARCH=x86`, `x64`, or `arm64`, and use a fresh directory per architecture
and configuration. Whole production translation-unit/full runtime builds remain
separate gates. The console callback is storage-audited rather than extracted into
this fixture.

For a preserved localization reference, `extract_save.py ROOT OUTPUT SOURCE_CPP`
extracts that original method instead. The original Mac implementation fails the
exact-byte fixture because it writes four-byte native `wchar_t` data under a two-byte
BOM. A reference compile intentionally omits `-Wshorten-64-to-32`, preserving the old
conversion for comparison; candidate builds keep strict shortening diagnostics.

Five additional Half-Life cases exercise the actual basename, one-newline removal,
server-response allocation, spectator name and localized mode-message bodies.
Lengths that remain native storage use `size_t`; the server API's signed count
uses the shared checked addition while retaining its 100-byte append reserve and
NUL. The fixed name and mode buffers terminate before conversion to int. Empty
newlines and separator-free basenames no longer read before the supplied string.
Long display strings are bounded where the old code overflowed its local buffer;
representable existing output and the single-newline rule are retained. The
key-trim and weapon-model length declarations are checked in separate full owning
translation-unit compiles, rather than represented by replacement test logic.

The localization extraction includes the selected legacy text-cell encoder's
header, matching the current owning source. These headless fixtures do not load
fonts, render UI, contact servers or qualify full client behavior.
