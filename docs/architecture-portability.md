# Future x64 and ARM64 portability inventory

The implemented build baseline targets **Windows x86 only**. Its source
translation has not yet been Windows-compiled. x64 and ARM64 are future porting
work, not selectable supported configurations. Future operating-system targets
have not been specified. CMake rejects unsupported host/architecture choices
rather than selecting a mismatched library or substituting a success stub.

## Source and ABI boundaries

| Area | Current evidence and required future work |
| --- | --- |
| Inline assembly | A static token scan of retained non-external C/C++/headers found no `__asm`, `__asm__` or `asm(...)` blocks, and no assembly input is selected by the retained projects. This bounded source finding does not make libraries architecture-neutral or rule out all compiler-specific constructs. Do not introduce raw x86 assembly shims. |
| x86 intrinsics / CPU discovery | `tier0/cpu.cpp` and `engine/sys_dll2.cpp` use `__cpuid`; `sys_dll2.cpp` uses `_mm_empty`; `engine/common.cpp` uses SSE conversion intrinsics. `engine/r_surf.cpp`, retained through swds, uses MMX `__m64` operations. Replace or isolate unsupported paths with faithful implementations for ARM64, with explicit FP/rounding behavior; do not silently stub CPU/audio/render functionality. |
| Pointer/integer width and loaders | `engine/LoadBlob.cpp` encodes addresses in `unsigned long` fields and casts loader/thunk function addresses through `DWORD`; it assumes Windows 32-bit image layout. A 64-bit build cannot fix this by widening every stored field. Audit pointer-bearing integers, callbacks, varargs/format strings, offsets, allocation arithmetic and serialized versus in-process addresses throughout retained client/server/UI code. |
| In-process interfaces | Engine/client/server function tables, virtual interfaces, HWND/module handles, STL/container layouts and private copied interfaces cross module boundaries. All in-process modules/providers must agree on architecture, layout, toolchain and CRT. An x86 stock client or server DLL cannot load in a native x64/ARM64 engine. |
| Calling convention and exports | `engine/eiface.h` uses Windows `__stdcall`; `.def` files include x86-decorated C++ names and ordinals. VGUI1 has a data alias at ordinal 395 and names-only import-library generation. Plan and verify each architecture's exports/imports and calling convention instead of reusing x86 decoration or relying on auto-export. |
| Packing/alignment | `common/const.h` contains pack(2) regions; `engine/BaseSystemModule.h` and `webm_video.h` contain explicit packing choices. Audit wire/disk structures separately from native class/ABI structures; preserve required format sizes while allowing native pointer width where appropriate. |
| Floating point | Preserve hardware's precise FP, strict `common.cpp`, SSE2/rounding/denormal assumptions, per-file EH and target-specific optimization. A3D has its own mixed precise/strict/fast per-file policy. ARM64 scalar/NEON code must reproduce required conversion/order/rounding behavior rather than globally enabling fast math. No ARM/x64 numerical equivalence is claimed. |
| OS APIs | Windows loader, registry, sockets, threads/synchronization, diagnostics, GDI, resources, DirectDraw/DirectSound/WinMM/OpenGL and COM interfaces remain dependencies. Some VGUI window data paths already use `GetWindowLongPtrA`/`LONG_PTR`, but this is not a complete width audit. Each future OS needs an explicit platform/provider design. |
| Compiler/runtime | Current static and DLL CRT selections differ by module. MSVC-specific SEH/async EH, assertions, intrinsics and linker exports need architecture/compiler review. The x86 flags are isolated in this baseline's target modules and VGUI import step, not advertised as reusable architecture-neutral settings. |

## Provider availability and rights

| Dependency | x86 evidence | x64 / ARM64 status |
| --- | --- | --- |
| SDL3 | Pinned native SDL3 3.4.20 unmodified headers, x86 COFF import archive identifying SDL3.dll and bundled x86 DLL; same upstream binary for Debug/Release consumers | Compatible native binaries, integration and redistribution review required; no matching libraries are selected or validated here |
| Steam / Steamworks | Bundled x86 steam_api.lib; inherited header provenance statement and runtime-loaded interfaces | Exact required interfaces, native provider availability, bitness, lifecycle and applicable SDK/redistribution terms must be established for each architecture |
| Miles | Bundled x86 mss32 import archive; original engine music source/worker/command integration retained | Native replacement/provider availability and rights unresolved. Port requires an authorized provider strategy that preserves behavior; no no-Miles stub is part of this task |
| Brotli, Ogg/Vorbis, WebM/VPX, bzip2, FreeType, Expat, PNG, zlib | Supplied x86 static-md Debug/Release libraries and retained notices | Rebuild/procure each native architecture with matching CRT/options and transitive dependencies, carry notices, then qualify media/font/codec behavior |
| VGUI / UI libraries | Source-built VGUI1/VGUI2/GameUI plus x86 prebuilt game_controls/vgui_controls archives and private interface copies | Native source/build availability for every selected object, interface layout and licensing need verification; x86 archives cannot be linked into 64-bit targets |
| Chrome / voice / optional legacy modules | Runtime loading paths exist; supply is not established by the CMake graph | Native compatible module/codec availability and redistribution rights unresolved; absence must remain visible |
| Aureal A3D | Preserved optional adapter source, x86-oriented API/export setup and legacy runtime loader | Provider/API availability and rights unverified; do not equate building an adapter with modern-architecture support |

`external/installed/x64-windows` contains package/build tooling, not a proven
complete native x64 dependency set for this runtime. No architecture may link a
32-bit library into a 64-bit target. Architecture selection must fail while a
required dependency remains unavailable.

## Compatibility and qualification work

Network packets, protocol fields, demos, saves, BSP/model/sprite formats and
other disk/wire representations must keep their specified widths, byte order,
packing and interpretation independently of native pointer size. Convert at
boundaries; do not serialize native pointers or widen formats to make an ABI
compile. Blob/image loading is a particularly clear example requiring a format
versus process-address distinction.

A future port needs separate evidence for native compilation, static PE/import/
export/ABI inspection, permitted module/provider loading, and actual CS/Half-Life
client/server behavior. Neither this inventory nor a green build establishes
runtime/playability acceptance. No future-architecture implementation, runtime
probe or game launch was performed.

SDL2 provider artifacts and the two platform VGUI binaries with formal SDL2 load
dependencies have been retired. `linux/release/vgui.so` has only libc, libgcc_s
and libstdc++ dependencies and remains retained evidence, without a functional
compatibility claim. Non-Windows SDL3 stream/ring source is retained and tested
at its arithmetic/ownership boundary; no native Linux/macOS runtime is qualified.
Windows DirectSound/WaveOut/Miles behavior remains a separate boundary.
