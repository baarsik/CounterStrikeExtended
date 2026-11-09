This packet integrates the qualified UTF16-cell formatter at existing native
caller boundaries. Windows-selected calls remain unchanged; standard swprintf
has no global alias. No UI, preferences, provider, game or network executes.

The C++-only POSIX prefix includes the formatter within extern-C++, so legacy
headers may include the prefix from extern-C. C sources receive no wide aliases.
Native _snwprintf/_vsnwprintf select the helper; local libc aliases are removed.
CS shared wrappers keep their existing ring, append, NUL and count policy. HUD
and chat no longer invert s/S or write into a localized resource. Career wrapper
formats return to their canonical Windows forms. The bounded 64/16-cell standard
numeric calls select the helper explicitly on native; generic standard-swprintf
semantics remain distinct.

Reproduce:

```
python3 tests/native/wide_format_callers/qualify.py \
  --output /absolute/new-receipts --build-root /absolute/new-builds
```

Release, Debug and ASan+UBSan build C++14 ARM64/minimum macOS14 fixtures. Exact
production HUD/chat branches, the complete voice label rebuilding method, the
GameUI numeric call statements and shared wrapper bodies are extracted without
semantic rewriting. Controlled localization/label bindings do not execute an
actual UI or qualify those services. Controls cover repeated mutable/const-backed
formats, UTF16 precision including a split surrogate, raw legacy truncation,
Valve numbered-placeholder dispatch, custom voice/fallback/name-only formatting,
wrapper ownership/count/extra-NUL policy and signed32 extrema in the proved
standard-call capacities. Prefix controls compile actual C and extern-C-wrapped
C++ includes; aliases link to the real helper and swprintf remains unaliased.

The original source produces two recorded REDs: missing native underscore-wide
aliases at compile time, and actual chat localization-buffer mutation. The
existing formatter wrapper test no longer remaps vswprintf artificially, so it
exercises the corrected native wrapper call itself.

windows_tokens.py compares real Clang preprocessor output with Windows/MSVC/Debug
symbols, for client/server selections. Includes are elided, so this is selected
body/header token invariance, not a new MSVC build or full-header qualification.
Clean Windows production builds remain a separate gate. Engine KeyValues uses
an exact alias-only after-copy; directory/representation work is separate.

compile_callers.py freezes whole source-TU copies and replays actual selected Mac
Release/Debug command rows; the original quote-include directory is restored.
This checks one selected owner per source/profile and compiles only. In the first
recorded pass, 18/24 compile: shared/voice/HUD/chat and five GameUI sources pass in
both profiles. Engine and CS KeyValues stop at direct.h; VGUI2 KeyValues stops at
an unrelated narrow I64 snprintf and if(!this) diagnostic. Those portions belong
to the parent's separate KeyValues work. game_shared/shared_util.cpp is not in the
selected Mac database; its obsolete local native alias removal is source cleanup
only. No extra source ownership or link acceptance is inferred.

Caller-owned raw legacy truncation is retained. In particular, the extracted HUD
512-cell branch can fill every cell without a terminator on oversized output,
matching its existing Windows call. This packet does not claim subsequent UI
consumption is safe for unterminated oversized text and introduces no new caller
termination policy. The qualified helper's explicit encoding/numeric contexts
and hardware-FP-status limitations continue to apply.
