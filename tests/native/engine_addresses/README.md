# Engine address boundary fixtures

These standalone fixtures compile the actual production `progs.h` header, complete
`R_StudioVertBuffer` and `GL_MakeAliasModelDisplayLists` methods, and the two small
address helpers. `extract_production.py` copies their C++ without changing it. The
alias/final-vertex/aux-vertex declarations and relevant constants also come from
their actual production headers. This does not compile or qualify the complete
renderer or engine translation units.

Coverage:

- The real edict is standard layout. Its area link follows the two 32-bit prefix
  fields at offset 8, contains native prev/next pointers, and leads back to the
  correct high-address object while traversing a three-edict ring. The macro's
  input is evaluated once. All four `world.cpp` uses and the `sv_user.cpp` use
  recover this same `edict_t::area` member. No edict field or external interface
  changes.
- The complete studio buffer method selects a stable, writable, 32-byte aligned
  high-address buffer for all `MAXSTUDIOVERTS` actual 32-byte vertex records, and
  preserves its auxiliary/light pointer bindings. The extra retained array
  element remains allocated.
- The complete alias method reads controlled cached command/order records and
  allocates from one high-address region. It must retain exact command bytes and
  pose byte order, including after relocating the entire header/data span, as
  `Mod_LoadAliasModel` does. The actual pointer-free header stays 268 bytes, with
  32-bit pose/command offsets at 92/96. The normal engine hunk allocator uses
  integer sizes and one contiguous hunk; no offset/header/cache width is widened.
  Native fault cases reserve a contiguous region larger than 2 GiB, commit only
  the touched pages, and return a command or pose allocation at `INT32_MAX + 1`.
  The method must reject before updating the corresponding field or payload.
  Separate helper cases cover both signed limits and the first values outside
  them. Win32's valid pointer differences already fit the retained signed int.
- Fixed flatcolor cases retain the ordinary Win32 signed low-32-bit hash colors
  and normalize negative remainders into the palette range. Cases around the
  sign/shift boundaries exercise the actual `hlRGB` palette consumer in both
  15-bit and 16-bit modes. Native high bits must not affect the result. The helper
  explicitly sign-extends the logical shift, avoiding implementation-defined
  signed casts/shifts. The frozen reference uses the exact old inline expression
  and must fail the formerly negative cases before attempting a palette lookup.

External alias file/hunk/logging services are fixture bindings. Cached triangle
generation, real file I/O, normal hunk management, OpenGL/software drawing, model
loading integration, full engine builds and gameplay are not qualified here.

From a matching x86/x64/ARM64 MSVC command prompt with Python, Ninja and the SDK:

```powershell
cmake -S tests/native/engine_addresses -B C:/CI/CSE-EngineAddresses/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=x64
cmake --build C:/CI/CSE-EngineAddresses/x64-Release --parallel
ctest --test-dir C:/CI/CSE-EngineAddresses/x64-Release --output-on-failure -V
```

Use separate directories and matching compiler prompts for x86/x64/ARM64 and
Release/Debug. Native region and static-buffer addresses are printed/checked
above 4 GiB; Win32 runs the actual production paths at ordinary 32-bit addresses.

To compile an archived original snapshot, copy its unchanged four source files
to a separate directory and add
`-DCSE_ENGINE_ADDRESS_SOURCE_DIR=C:/CI/CSE-EngineAddresses/before`
`-DCSE_ENGINE_ADDRESSES_LEGACY=ON`. Its visible diagnostics are retained: this
option allows compilation of the old narrowing casts without suppressing them.
On macOS it enables Clang MS extensions for those old casts. Native studio and
both alias range tests must fail against the archived original; a Win32 original
run supplies the actual 32-bit reference for the remaining byte/index behavior.
The archived original has no offset guard helper, so that scenario explicitly
reports the missing helper; the complete-method fault cases detect the defect.
The edict macro may already run successfully despite its obsolete null-member
pointer cast; the compile diagnostics motivate that behavior-preserving repair.

macOS ARM64 uses the same commands with local directories, for example
`build/engine-addresses-macos-arm64-release` and `-DCSE_TARGET_ARCH=arm64`.
ASan/UBSan is available with Debug and
`-DCSE_ENGINE_ADDRESSES_SANITIZERS=ON`, in a separate build directory. No game,
microphone, full project build or VM action is performed by these fixtures.

All production changes are inside existing files. No new production build
membership or retained-manifest addition is required.
