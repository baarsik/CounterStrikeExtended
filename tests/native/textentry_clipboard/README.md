# TextEntry clipboard caller controls

These fixtures construct the complete real common/CS TextEntry types. They import
the actual constructors/destructors, Paste, CopySelected, InsertChar, SaveUndoState,
FireActionSignal, keyboard entry points and small supporting methods. There is no
replacement widget, access macro, raw-offset access or installed vptr. A legitimate
derived recorder and typed private-member access expose controlled test state.

Panel construction/destruction, layout, focus navigation and unrelated virtuals are
explicit controlled definitions. Unexpected calls fail through a separate typed
binding, rather than becoming successful stubs. Assertions also fail locally so a
bad fixture cannot open a Windows assertion dialog. Forced Steam initializers reach
only a counted test binding; no actual provider starts.

`map` uses the actual registered message entries, Panel::OnMessage and a separately
compiled actual VPanel.cpp under its own declaring recipe. The dictionary and
IPanel adapter remain controlled. `chain` uses direct controlled routing and is a
separate gate. Neither qualifies actual tab navigation or renderer layout. Real
KeyValues construction, allocation, name access, destruction and deleteThis are
retained; unrelated getters fail. Allocation/free counts prove the synchronous
receiver leaves DoPaste owned by its sender.

The widget allocator binding replaces only DefaultAllocator, allowing authored
memory, allocation failure and exact ownership checks. Buffer/Frame bodies stay
actual. The standalone helper gate compiles and executes the unchanged helper,
including its real default allocator, with C++14. `run_mutations.py` requires ten
contained semantic mutants to compile and then fail actual-widget assertions.

Run from the checkout with the recorded owning databases:

```sh
python3 tests/native/textentry_clipboard/run_mac.py --repo "$PWD" --out /path/to/artifacts/common \
  --database /path/to/compile_commands.json --owner common --test all
```

Use `--owner cs` for the second real declaration and `--sanitize` for ASan/UBSan.
`whole_tu.py` compiles all six real consumers and new helper memberships in R/D.
`run_windows.py` consumes pinned actual Ninja recipes in an isolated VM directory;
ARM64 uses a labeled component profile, not an owning ARM64 full-build claim.
`whole_windows.py` preserves all existing Win32/x64 caller policies and adds Sync
only to the helper. No production warning/sanitizer suppression is introduced.

The two-image controls use distinct public POD API symbols. They show why both
file-local TLS and noninterposable helper code are required: the original and
anonymous-TLS-only controls fail flat binding; final two-level/flat R/D/sanitizer
images have independent chains and no private helper exports. The early
same-public-symbol shim was confounded and is not qualification evidence.

No clipboard, window, renderer, real provider or game executes. The private helper
requires C++ exceptions to unwind a throwing reader. Production callback/receiver
unwind is not qualified in the unchanged NoEH hw_engine/swds callers. Widget
lifetime during callbacks, void setter publication failure, cross-call snapshot
atomicity, actual navigation/layout and full UI acceptance remain separate gates.
