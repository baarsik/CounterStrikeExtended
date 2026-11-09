This component exposes the coherent absolute mouse position and center through
the owned engine/client C tables. Runtime ABI10 requires rebuilding all owned
modules, including Win32; the 288-byte descriptor envelope is unchanged. Network
and save formats are unchanged. Camera ownership and cursor policy are separate.

`prepare_fixtures.py` extracts the actual engine wrapper, typed default destination
and positional source-table initializer. Unrelated initializer entries use typed,
nonexecuted services in a private namespace. The existing ABI fixture supplies
actual engine record declarations. SDL/video functions are controlled services;
no SDL library, window, physical input or other provider is executed.

Checks cover the actual new source and destination prototypes, initialized table
entries, pointer redirection through the destination hook, default destination,
outputs above 4 GiB, missing window, bad output pointer and dimensions, nonfinite
input, and failure without a partial output commit. The coordinate component
separately covers the arithmetic corpus. The client ABI suite checks both actual
client initialization copies, cross-module high-address output calls and refusal
of equal-storage prototype changes before function exchange.

Configure with `CSE_SDL_INCLUDE` pointing to pinned SDL3.4.20 headers, then build
and run CTest. Use separate Release/Debug directories and
`CSE_MOUSE_CALLBACK_SANITIZERS=ON` for the sanitizer profile. All are standalone
fixtures. `run_negative_variants.py FROZEN_SOURCE NEW_OUTPUT` checks five private
source mutations; every variant must compile, then fail its behavioral checks.
