# Pure VGUI2 clipboard envelopes

`NativeClipboard.h/.cpp` is an unselected integer/byte component. It has no allocator, registry, clipboard API,
provider, worker, global mutable state, lease implementation, or publication path. The caller must own immutable
input/metadata and real readable/writable typed spans for the duration of each call. Service/context/module
lifetime, acquisition/release, thread rules, callers and selected System wiring remain separate gates.

The two snapshot views are independent. Count validates only its legacy allocation-byte count. Raw reads copy
UTF16LE allocation bytes without text validation. Wide reads use an explicit even unit extent within that
allocation, require a NUL somewhere in the full extent, and widen complete UTF16 units into actual wchar_t cells.
A short destination need not fit the source terminator. Embedded-NUL tails are copied; no read trims or forces a
terminator. Wide offsets are units. Odd destination capacity leaves its incomplete-cell bytes untouched. Odd
allocation padding is available to raw reads but is excluded from the declared complete-unit extent.

Every returned failure status preserves the whole destination and out-count. Later legacy wrappers must initialize
their result to zero and map a failed status; no wrapper is supplied here. Exact-end reads succeed with count 0;
past-end offsets fail. Relevant input/output/out-count overlap is rejected. Packing validates the full explicit
input prefix before writing, preserves raw8 bytes/raw16 units, expands valid native32 supplementary scalars, and
appends one NUL byte/unit. Embedded NUL never stops packing. Invalid scalars reject transactionally.
`PlanLegacyBytes` is length-only size planning; it does not claim source storage exists. Private pack byte results
use uint64; public read/count results must fit signed32. No external narrow encoding is selected.

These are intentional native divergences from the selected Windows implementation's mixed byte/cell offset,
odd partial-cell writes, positive counts without copied data, int-wrapped sizes, unbounded reads and leaked or
cleared-before-failure publication ownership. This component does not modify Windows source or implement service
failure handling. It does not repair TextEntry's CR length, CS Paste termination/overflow, or auto-progress tail
publication, and does not claim UI acceptance.

Run the pure Mac tests and comparison:

```text
python3 tests/native/vgui2_clipboard/run_mac.py --repo . --art <new-packet>
python3 tests/native/vgui2_clipboard/run_mutations.py --repo . --art <new-packet>
python3 tests/native/vgui2_clipboard/compare_reference.py --original <sealed-design-packet> --art <new-packet>
```

The comparison requires all six Windows profiles from `run_windows.py` as well as Mac R/D/sanitizers. Stage the two
production files and focused tests with repository-relative paths, then pass that stage to the Windows runner.
All compiler commands/identities are recorded. C++14 is explicit; there is no global standard/policy change.

Each profile packs and widens all 65,536 raw units, checks raw-byte prefix/offset/capacity matrices, full failure
guards/counts, overlaps, exact physical cells, malformed/missing views, result/range limits and high owned native
addresses. Native32-only scalar cases run on actual Mac wchar32; Windows uses its real wchar16, with pair units.
The final matrix also rejects oversized result metadata using owned no-access pages before any read/write; those
claims are deliberately not backed huge data spans or successful multi-GiB transfer evidence. Win32 skips that
native64 metadata case and the impossible above 4 GiB requirement. No clipboard API is called by these tests.

Fourteen admitted byte/unit/count/explicit-pack observations per profile compare to a sealed actual Win32-method
reference. Whole physical wchar16/wchar32 bytes are not compared as if their layouts matched. Mac valid-scalar
rows additionally compare with an independent Python UTF16LE encoder. Deliberate failure/partial-cell divergences,
OS publication, clipboard acquisition, lease behavior and full System/UI operation are outside that parity set.
