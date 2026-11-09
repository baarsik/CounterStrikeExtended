# First authored offline shader adapter

This standalone fixture reads the frozen private DrawPacket/texture/raster
contracts. It does not hook qgl/raw GL, select a production backend, change a
manifest, run a game, or compile shader source at runtime. The existing six
legacy capture files remain unchanged.

`gpu_capture` owns prepared vertices/constants and retains immutable captured
images. `gpu_adapter` borrows a caller-owned SDL device only for a synchronous
offscreen call. It owns its pipeline, shaders, staging, textures, samplers and
fence, waits for completion, copies readback bytes and releases those resources.
The initial implementation deliberately allocates per call; it is not a
production batching/cache/performance claim.

The original HLSL uses SDL's documented bindings: vertex uniform b0/space1,
fragment uniform b0/space3 and fragment textures/samplers t0..2/s0..2 in space2.
Actual FXC output reflects 320 vertex constant bytes and 144 fragment constant
bytes, one block per stage, three sampled textures and no storage resources.
C++ layout assertions and `verify_offline.py` compare the exact generated
containers/assembly against the compiler receipt. This is actual compiler
resource/layout evidence; it does not establish pixel equivalence for every
represented shader branch.

`compile_fxc.py` uses the existing pinned Windows SDK10.0.26100.0 FXC executable
with warnings-as-errors and separate Release O3 / Debug Od+Zi, vs_5_1/ps_5_1.
It writes new output directories only, records compiler/source/container hashes
and preserves diagnostics. No shader compiler/provider upgrade is performed.
The initial multi-return helper's FXC X4000 failure is retained externally; an
initialized-result/single-return rewrite compiles without warning suppression.

The first V1 real proof used Interactive Limited Windows x64 execution on the
Parallels Display Adapter (WDDM), SDL3.4.20 D3D12/DXBC with explicit fewer resource
slots. One smooth triangle-list packet, a nearest RGBA8 replacement texture,
identity texture transforms and a dyadic modelview half-scale produced exactly
the frozen real-GL 64x64 reference: 12,288 RGB bytes, no differences. Clip Z is
converted from GL [-w,w] to GPU [0,w]; only GPU readback row order is reversed for
comparison with GL's bottom-first bytes. Primitive colors and texture bytes are
unchanged. The adapter never changes the earlier Windows flat-lowering RED.
Exact later source/configuration coverage belongs to the frozen qualification
receipt; a successful CPU CTest run is not a GPU render receipt.

Release/Debug matrix probes on VM1 compare three active texture units and the
selected linear-fog/blend/scissor profile exactly against actual GL RGB output.
Alpha tests, depth and the other represented shader/pipeline branches still
require their own measured GPU comparisons. Flat shading, nontriangle topology, rectangles,
palette textures, low/unsized precision, mip filters, legacy border clamp,
anisotropy, projective texture matrices and depth offset are rejected by the
first profile. They are explicit unfinished renderer features, not silently
approximated or substituted. No full-game or general GPU parity is claimed.

Offline Metal compilation supports the user-supplied Windows Metal6.2 package
and native macOS Metal Toolchain27B47. `compile_metal.py` verifies the selected
compiler/linker and header inventory (`metal_6_2_windows.json` or
`metal_27b47_macos.json`), including native executable/directory alias mappings
before and after compilation, then produces separate vertex and fragment metallibs
for Release O2 and Debug O0 with line information. MSL3.1
and macOS14 deployment are explicit; unsafe math, contraction and fast FP32
functions are disabled. `verify_metal.py` checks the producer receipt and AIR
metadata: the same 320/144-byte uniforms, member offsets, five vertex attributes
and three texture/sampler pairs. AIR metadata is a compiler output alongside the
object; it is not a claim of complete metallib reflection or pixel parity.

The private `legacy_first.metal` counterpart uses SDL's documented Metal buffer0
uniform bindings and stage-in vertex attributes. On actual Apple M5 Max hardware,
Release, Debug and instrumented CPU preparation/adapter builds each match the
first frozen Windows GL reference exactly: 12,288 RGB bytes. Only the readback
row order is normalized. The Metal shader's other represented branches still
need their own pixel comparisons; the Windows three-profile results do not
qualify those branches on Metal.

That first Metal run exposed the pinned SDL3.4.20 upload implementation ignoring
`pixels_per_row`: padded two-pixel rows read zero padding as the second row.
The RGBA8 adapter now supplies tightly packed upload rows to the actual Metal
backend while retaining padded D3D12 transfers. The old padded-row control
reproduces 4,096 differing RGB bytes; the corrected upload passes. Readback
continues to use its explicit pitch. SDL's version and source are unchanged.

The Windows tools run in VM1 and generate target-specific files for native Mac
execution. After the user configured Xcode, Apple's component downloader supplied
native Metal Toolchain27B47 (compiler32023.922); its libraries also match the same
first packet in all three native fixture profiles. The earlier Windows-produced
packet/compiler evidence is retained separately. The global developer directory
stays at CLT; Xcode discovery uses per-command DEVELOPER_DIR. No shader source is
compiled at game/fixture runtime and no stock shader is substituted. The
production renderer remains unselected.

Configure this fixture with matching pinned source-built SDL3 library/runtime
paths, C++14 and the desired native architecture. CPU tests also accept the
actual offline FXC directory for resource/layout checks:

```sh
cmake -S tests/native/render_gpu -B build/render-gpu-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_RENDER_GPU_SDL_LIBRARY=/absolute/pinned/libSDL3.0.dylib \
  -DCSE_RENDER_GPU_SDL_RUNTIME=/absolute/pinned/libSDL3.0.dylib \
  -DCSE_RENDER_GPU_OFFLINE_DIRECTORY=/absolute/actual/fxc-output
cmake --build build/render-gpu-debug
ctest --test-dir build/render-gpu-debug --output-on-failure -V
```

For Metal, set `CSE_RENDER_GPU_METAL_DIRECTORY` to the complete producer output
and run `render_gpu_probe <output/Release-or-Debug> <frozen-reference.rgb>` on
macOS. The compiler producer runs on Windows with a new local output directory:

```powershell
python tests/native/render_gpu/compile_metal.py --toolchain C:/path/to/MetalTools `
  --source GoldSrc-SDK/engine/render/native/shaders/legacy_first.metal `
  --output C:/new/offline-shaders
```

On macOS, pass the enclosing `Metal.xctoolchain` directory and a new absolute
output directory to the same producer. Discover the installed executable with
`env DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcrun --toolchain Metal --find metal`.
The toolchain version and file hashes must match the checked-in macOS pins.

`CSE_RENDER_GPU_SANITIZERS=ON` instruments owned Clang CPU code. On Windows use
`CSE_TARGET_ARCH=x64`, matching SDL import/runtime paths and an MSVC environment.
GPU execution is a separate explicit desktop action:
`render_gpu_probe.exe <matching-config-DXBC-directory> <frozen-reference.rgb>`.
The driver and exact pinned adapter are asserted; no WARP/software adapter is
requested or accepted. Keep the desktop lane coordinated with other fixtures.

On 64-bit Windows, `render_gpu_matrix_probe <matching-config-DXBC-directory>`
compares the three represented RGBA8 raster profiles against live original GL
and owned-snapshot GL draws in the same process: replacement, fog/blend/scissor,
and three-unit detail scaling. It compares exact RGB bytes after row-order
normalization and records any differences. RGB5_A1 remains an explicit rejected
profile. This executable is a separate desktop action, outside default CTest;
building it does not qualify its pixels.

The matrix uses the explicit `texture-raster` mode of the GL oracle. The full
`textures` mode continues to test uploads and display-list deletion independently.
An initial Windows matrix attempt stopped in that full mode at the existing
immediate-deletion check, before any GPU rendering. That failure remains a
separate unresolved reference result; raster-only measurements do not qualify
the upload/list path or change its expected behavior.
