# Native VGUI1 Debug / Color component

The new `posix/Debug.cpp` supplies ONLY `vgui_dprintf` and `vgui_dprintf2`. It must be selected for the native
Mac target instead of `win32/Debug.cpp`; the Windows file stays byte-identical. No App, input, font, clipboard,
surface, provider, network or preferences behavior is selected by this component.

The retained Linux reference is `linux/vgui.cpp`, not a nonexistent Linux Debug.cpp. The owned file keeps the
bounded 2048-byte stack buffer and native `vsnprintf` result, then emits its C-string prefix through `fputs`
to stderr. There is no SDL logging, extra prefix/newline, heap buffer or truncation-return rewrite. An embedded
NUL stops emitted text while the formatter result still counts subsequent formatted bytes. Encoding errors
retain the formatter's negative result. Buffers are zeroed and their final byte is explicitly terminated.

Root's counter policy defines previously undefined signed overflow/data races: storage increments as uint32
modulo 2^32, with a safe signed32 interpretation for `%d`. The sequence crosses 2147483647 to -2147483648 and
-1 to 0. Both functions use `flockfile(stderr)` through formatting/emission, with matching `funlockfile` on the
single return path. Counter assignment and output are serialized. No mutex, destructor or module callback is
introduced. IO write failure is not substituted for the inherited formatter return value.

Source audit found zero retained callsites beyond declarations/platform definitions. No Windows-specific
formatter extensions were observed; no arbitrary MSVCRT format, wchar/locale or `%p` equivalence is claimed.
The byte control compiles the exact retained Windows function bodies with the SAME NATIVE formatter and an
owned OutputDebugStringA-to-stderr transport. It proves buffer/return/prefix handling, not Windows CRT behavior.

## Color declarations

The private Color already declares a default copy assignment. Its implicit copy constructor triggers strict
`-Wdeprecated-copy`. Both headers gain an Apple-only default copy constructor; the public header also needs an
Apple-only default assignment to avoid creating the reverse warning. The private assignment is unchanged.
These are nonvirtual defaults, not byte-punning replacements. No field, virtual slot, RGBA conversion or alpha
policy changes. Windows-selected preprocessed tokens match baseline exactly in both contexts, and the original
non-UTF8 copyright bytes are preserved.

Actual Color.cpp is linked separately with each declaration context. The only App binding aborts if unexpected
scheme-backed RGBA resolution occurs; scheme-enum copying is tested separately. The fixture uses normal C++
copy/assignment, properly aligned byte-array placement storage, byte guards, and legal object-representation
READS. It never misaligns a polymorphic Color or overwrites its representation. Derived/base slicing and base
assignment verify real virtual dispatch, avoiding whole-object memcpy semantics. Baseline/post results retain
size16, alignment8, RGBA offset8 and all measured copy/move/noexcept/triviality/polymorphism traits.

## Reproduce

```sh
cmake -S tests/native/vgui1_debug -B build/vgui1-debug-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/vgui1-debug-release --parallel 2
ctest --test-dir build/vgui1-debug-release --verbose
cmake -S tests/native/vgui1_debug -B build/vgui1-debug-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/vgui1-debug-debug --parallel 2
ctest --test-dir build/vgui1-debug-debug --verbose
cmake -S tests/native/vgui1_debug -B build/vgui1-debug-san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DCSE_VGUI1_DEBUG_SANITIZERS=ON
cmake --build build/vgui1-debug-san --parallel 2
ctest --test-dir build/vgui1-debug-san --verbose
```

The pipe fixture captures exact stdout/real stderr for empty, short, 2044..2048, long, prefix-width transitions,
embedded NUL, encoding failure, signed-counter boundaries and 2048 concurrent records. Python drains both
pipes; concurrent stderr exceeds 16KiB (about545KiB), with exact ordered counter identities and no interleaving.
Color's strict failure was captured before edits, then both contexts pass without deprecated-copy suppression.
All tests are bounded. Release/Debug/ASan+UBSan and retained owning VGUI Release/Debug Debug/Color/Panel TU
compiles are separate receipts. The Debug TU uses only source/output substitution in the owning old Debug
command. The other two use actual owning source commands with output substitution.

Current execution is macOS27.0.1 ARM64, Apple clang21, deployment target14. This does not qualify actual macOS14,
a complete VGUI1 module, UI/game behavior, Windows CRT formatting, or other platforms. Complete VGUI1 acceptance remains separate from these Debug/Color checks.
