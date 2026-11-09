This is an unwired, decimal-only compatibility boundary for the retained UCRT's
32-bit `strtol` / `strtoul` values, byte endpoints and `errno` behavior. It does not
implement a general base API, wide conversion, locale mutation or CRT invalid
parameter handling. Input must be non-null and NUL terminated. A null end-pointer
argument is supported. Consumer integration is checked separately.

`CseLegacyStrtol10` returns `int32_t`; `CseLegacyStrtoul10` returns `uint32_t`.
Both use the sealed `legacy_text_to_integer.h` byte classifier without changing
that header. The default C/UTF8 profile recognizes ASCII decimal digits and ASCII
whitespace (09..0D and 20). The explicit SingleByteNbsp profile also recognizes
byte A0. UTF8-encoded NBSP is not decoded into a single-byte whitespace character.

The measured rules are:

- Leading whitespace and one optional sign precede decimal digits. Base inference
  does not occur: `0x17` converts zero and stops at `x`.
- No digits returns zero, leaves the endpoint at the original input (before any
  whitespace/sign), and preserves the incoming `errno`.
- Success preserves `errno`. Endpoints are byte positions; no copied input or
  persistent borrowed pointer is retained.
- Signed magnitude overflow returns INT32_MIN/MAX as appropriate. Unsigned
  magnitude overflow returns UINT32_MAX before any sign negation. Both set the
  platform ERANGE and consume the complete digit run, including after overflow.
- Unsigned in-range negative magnitudes negate modulo 2^32: `-4294967295` yields 1,
  while `-4294967296` yields UINT32_MAX/ERANGE. A wider libc conversion followed by
  narrowing cannot reproduce this contract.

The parser uses checked unsigned cutoff arithmetic. It never negates INT32_MIN,
performs overflowing signed arithmetic or converts out-of-range unsigned values
into a signed result. It has no global state and does not consult the host locale.

The reference fixture calls linked UCRT functions and their explicit per-call
locale variants on Windows x86/x64/ARM64 Release/Debug. It records actual compiler,
provider path/version/hash, loaded PE machine and probe identities. No msvcrt.dll
exports or global/per-thread locale changes are used. C, explicit C and UTF8
profiles agree; the explicit .1250 profile differs only by its A0 whitespace rule
in the exhaustive byte controls. Each table has 8,880 rows from 1,110 authored byte
inputs, both signednesses, two incoming errno values and both end-pointer modes.
The corpus covers limits, negative unsigned limits, 64-bit limits, 10,000-digit
runs, no conversion, embedded NUL, all 256 bytes before/after digits and relevant
non-ASCII UTF8 byte sequences.

Mac LP64 libc controls deliberately fail against the Win32 reference: Darwin
sets EINVAL on no conversion, and its wider long range changes values/errors.
These REDs are retained independently of the new helper's GREEN comparisons.
The tests compare all result bits, byte offsets and semantic ERANGE; they do not
assume that errno numeric encodings must match on every operating system.

`extract_scanf.py` extracts the whole actual VaScanf methods from engine,
VGUI2-public and cstrike-public, adapting only their private fixture class names.
A typed fixture binding replaces the two decimal CRT calls with the new helpers;
all retained methods still pass base 10. The storage stand-in owns unsigned bytes
and records cursor/count/error state. String and explicit format-whitespace paths
fail immediately if invoked; their behavior is outside this fixture. The original
float call remains linked but unexercised. This is actual method-body coverage,
not a whole UtlBuffer TU/ABI or UI qualification. It proves assignment-before-count
on no conversion, prior successful count retention, unchanged failed-conversion
cursor, completed overflow conversion, explicit decimal percent-i, and early
invalid/binary-buffer rejection.

New helper/corpus code requires C++14 and strict conversion warnings. The extracted
legacy methods use Wall/Wextra/Werror plus the owning width-error checks, because
an unselected existing unsigned-byte percent-c assignment triggers the extra
Wsign-conversion option. The initial strict-fixture diagnostic is preserved;
no production method, warning suppression or compiler-wide policy was changed.
MSVC fixtures retain /W4 /WX /EHsc. ASan+UBSan instruments the helper and extracted
consumer fixtures, with nonrecovering checks. No game, network, provider, desktop,
preferences or protected runtime producer is accessed.

Example native qualification, using the frozen actual x86 Release reference:

```sh
cmake -S tests/native/str_to_integer -B build/str-to-integer-helper-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCSE_STR_BUILD_HELPER=ON \
  -DCSE_STR_REFERENCE=/absolute/frozen/receipts/x86-Release
cmake --build build/str-to-integer-helper-release --parallel 2
ctest --test-dir build/str-to-integer-helper-release -V --output-on-failure
```

Repeat Debug and add CSE_STR_SANITIZERS=ON for the instrumented gate. The Windows
runner freezes inputs before compilation, initializes each CPU compiler environment
only in memory, probes the actual linked CRT, then compares that CPU/configuration's
helper against its own tables. No compiler environment is logged.
