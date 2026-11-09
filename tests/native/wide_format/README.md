This fixture qualifies an unwired UTF16-cell formatter against the linked Windows
UCRT. It changes no existing caller, manifest, dependency or Windows production
selection. The original design/reference packet remains immutable in
`ART/wide-format-final`; the implementation packet is separate.

The helper has a CRT-shaped variadic interface with legacy `_snwprintf` /
`_vsnwprintf` bounds: an exact fit returns the cell count without a NUL;
truncation returns -1 and fills the permitted cells. A null destination with zero
capacity is a length query. Null format, null destination with nonzero capacity,
and an unrepresentable object capacity fail with EINVAL before writing. Existing
caller-owned termination guards remain necessary. This is not a universal alias
for standard `swprintf` / `vswprintf`, whose truncation contract differs.

It accepts canonical Windows wide formats: s/ls/ws are wide, S/hs are narrow;
c/C and explicit length variants retain their measured cell/byte behavior.
Flags, dynamic width/precision, d/i/u/o/x/X/p, hh/h/l/ll/I32/I/I64/j/z/t/w/L,
and f/F/e/E/g/G/a/A are parsed before consuming typed arguments. Native long
arguments are consumed as native long and formatted as the explicit Windows
32-bit logical word. Pointer and I sizes follow the selected native pointer
width. Selected Mac ARM64 and Windows ABIs use binary64 long double; other
long-double representations deliberately fail the compile-time contract.

Output counts and widths use UTF16 cells. Existing surrogate cells are retained;
a native wchar32 supplementary scalar expands to two cells. Wide precision may
split that pair. Strict UTF8 narrow precision retains the measured UCRT scalar
iteration rule, including a two-cell supplementary result at precision one.
Invalid UTF8 strings fail atomically at their field with EILSEQ, preserving an
already-written prefix. Invalid narrow characters set EILSEQ, skip the character
and continue, as measured. Native wide values beyond Unicode's maximum are
rejected rather than silently truncated into a BMP cell.

The default context is strict UTF8 with independent C numeric punctuation. An
explicit CByteIdentity profile reproduces linked-UCRT C-locale byte identity.
Neither profile claims arbitrary Windows ACP/1250 or user-locale equivalence.
The implementation never changes process/thread locale, retains no borrowed
arguments, copies the va_list, allocates no dynamic storage, and propagates no
exceptions. Caller format/argument types, readable input lifetime, nonoverlap and
actual output object capacity remain the usual variadic API responsibilities.
Disabled n returns -1/EINVAL without writing its count target. Windows CRT
invalid-parameter callback APIs are not supplied by this native value boundary.

Finite numeric conversion decodes IEEE754 bits into a fixed bounded decimal
integer. Mantissa * 5^1074 bounds the negative-exponent case; the allocation bound
uses the minimum binary exponent, max_digits10 and one carry. Integer arithmetic
performs decimal/hex rounding. Excess requested precision streams zeros to the
bounded sink instead of allocating a precision-sized object. The current
thread's FE_TONEAREST/UPWARD/DOWNWARD/TOWARDZERO selection is read and preserved.
No libc numeric formatter, floating classification predicate or floating
rounding arithmetic is used. Hardware FP exception/status-flag side effects
remain unqualified.

Actual linked-UCRT controls establish three explicit compatibility quirks:

- Fixed decimal rounding before the first significant position drops magnitude
  even in directed modes. Named DEC-P controls include both signs, all modes,
  precision boundaries and neighboring binary64 patterns.
- A zero first discarded hex nibble suppresses a directed increment despite a
  later nonzero tail. Named HEX-P controls cover kept parity, carries,
  subnormals/exponents and neighboring patterns.
- Only the exact negative canonical quiet-NaN pattern receives nan(ind).
  Alternate precision-zero special output has sign-sensitive point placement;
  the negative form retains zero padding and a/A's hex prefix. Explicit special
  sign/width/precision controls pin this policy, including i.nf versus -.inf.

The implementation packet contains these actual gates:

- Windows x86/x64/ARM64 Release/Debug under MSVC 195136260. Both precise and fast
  core policies run with exceptions disabled. Each configuration compares 3,480
  wide rows, 2,288 byte/character rows and 441,752 numeric rows exactly against
  actual linked UCRT. Every numeric reference row separately checks real wide
  and narrow UCRT outputs before recording ASCII cells. Pointer-width cases are
  compared to the matching CPU reference.
- Mac ARM64/minimum macOS14 C++14 Release, Debug, ASan+UBSan, fast Release and fast
  Debug compare the same outputs against the native-width Windows reference.
  Four-worker fixtures check independent encoding, errno and rounding contexts;
  bounds/guards, va_copy reuse, temporary input ownership and INT_MAX precision
  are exercised without huge allocations.
- Eight actual generated Mac compiler-command replays substitute only the helper
  input/output: engine NoEH and client EH, Release/Debug, selected precise policy
  and explicit fast policy. Header inputs and object hashes are recorded.
- Exact production SharedWVarArgs and BufWPrintf bodies are extracted into the
  fixture. Their ring order, extra termination guard, append count and ownership
  are tested. This is method-body coverage, not a full UI module execution.
- The core's undefined-symbol inventory contains no allocation, locale or numeric
  printf dependency. Mac execution is on the recorded host, not a macOS14 host;
  deployment minimum and ARM64 Mach-O identity are checked separately.

The retained standard-swprintf routes need deliberate integration rather than a
global alias. Observed KeyValues calls use a 64-cell buffer: signed32 needs at
most 11 cells, signed64 at most 20, promoted binary32 fixed-six formatting at
most 47. LabelSliderCombo's 64-cell and CareerGameTasks' 16-cell `%d` buffers
also fit every signed32 value plus NUL. Their defined valid outputs therefore
fit both bounds contracts. Truncating generic/custom callers still require the
appropriate API contract. Parent-owned native s/S inversion loops must be
removed before these canonical Windows formats are selected.

Reproduce a local gate with:

```
python3 tests/native/wide_format/qualify_macos.py \
  --reference /absolute/windows-reference/x64-Release \
  --output /absolute/new-receipts \
  --build-root /absolute/new-builds
python3 tests/native/wide_format/compile_inherited.py \
  --artifacts /absolute/ART --output /absolute/new-policy-receipts
```

`windows_reference.py` requires a frozen source tree and source-hashes.json. It
copies them into a new physical guest workspace, initializes each compiler
environment in memory without logging it, builds/runs six configurations, and
pins provider hashes/versions/PE machine plus produced binaries. `--helper`
enables comparisons; `--fast-fp` changes only core compiler policy. All runs have
fixture-only deadlines. No UI, clipboard, preferences, game, audio or network execution is required.

Prior REDs are retained: mapped native libc string/termination controls; 114
numeric libc mismatches; finite threshold mismatches before the confirmed UCRT
policies; special layout mismatches; one MSVC lambda-capture fixture compile
error; and the standalone Clang precise/contract option-order diagnostic.
They are not suppressed. The actual inherited precise/off command policy is
replayed unchanged; the strict standalone fixture avoids redundant FP options.

The observed 108-file Steam localization corpus stays in ART, with source-path
and before/after hashes. It is reference evidence, not a complete format
allowlist or permission to drop custom/localized/chat formats. No third-party
resource text or numeric dependency was added to the repository.
