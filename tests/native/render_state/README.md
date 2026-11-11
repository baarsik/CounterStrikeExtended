# Private legacy renderer state: first chunk

This directory tests only the new portable CPU state/command layer. It is not wired into engine rendering, qgl dispatch, backend selection, window creation, or presentation. Existing OpenGL rendering remains unchanged. No SDL_GPU renderer, DXIL/metallib shader pipeline, pixel/backend equivalence or full-game acceptance is claimed.

Source-derived profile: `GL_Init` in `gl_vidnt.cpp` establishes alpha/depth/blend/shade state; `R_SetupGL` in `gl_rmain.cpp` postmultiplies matrices and reads them back; `DetailTexture.cpp` selects texture unit2 and scales its independent texture matrix; `r_triangle.cpp` exposes seven primitive modes and current color/UV rules; `EngineSurface::drawFlushText` uses position/unit0-UV arrays with current color; world surfaces in `gl_rsurf.cpp` use geometry `GL_COMPILE_AND_EXECUTE` lists. Test sequences are re-expressed from these sources; existing engine method bodies are not compiled by this fixture.

`LegacyState` stores column-major float matrices, postmultiplication, bounded modelview/projection/per-unit texture stacks, current color and three units of coordinates. `Recorder` owns geometry commands and list definitions. A draw snapshots matrices/raster state; vertex submission latches current attributes. Compile-only lists preserve caller state; replay uses the later matrices/current attributes. Replacing a list commits at EndList, and deleting a list cannot invalidate copied draw packets. VGUI's enabled arrays supply only positions and unit0 UVs; other units and color remain execution-time current state. Copies own all storage; there are no borrowed list/client-array pointers.

The raster profile includes enables, depth write/comparison/range, alpha reference/comparison, blend factors, cull mode, shading, viewport/scissor, fog parameters, polygon mode/offset, front-face orientation and fog/perspective hints. GPU framebuffer/presentation/readback execution and shader selection remain subsequent work. This layer is not a general OpenGL emulator. Its private checked limits are not advertised as driver limits. Standard container allocation exceptions remain possible; explicit limits/invalid values use Status. Replay depth/work and aggregate list/frame storage are bounded. Invalid matrix queries throw out_of_range rather than indexing outside storage. Unrepresentable projection coefficients and command scalar narrowing fail before conversion. A failed aggregate EndList or frame End retains pending owned storage for an explicit retry after capacity is released; replay limits may leave already executed state/draws, and are not transactional rollback.

Triangle lists/strips/fans and lines/line loops can be lowered with preserved winding and flat provoking color. Original quads/quad strips/polygons remain owned packets; lowering returns UnsupportedTopology. This deliberately does not establish a diagonal, interpolation or edge rule for those modes. Incomplete triangle/line tails are omitted during supported lowering.

The real oracle creates its own hidden SDL compatibility-GL context using pinned SDL3 3.4.20. It records actual provider strings, flushes each initialization stage, and on Windows records the loaded SDL DLL path. Run the Windows graphics oracle in the logged-in desktop session (Interactive Limited scheduled task); a direct noninteractive VM-exec session may expose a different legacy context/profile. Missing features remain an explicit failed probe. Matrix/current-state/raster queries and known representable transforms compare exactly. Feedback compares primitive attributes with oriented cyclic triangle alignment (never reversed winding): Apple GL feedback uses a different starting vertex. Feedback reports per-vertex colors before flat-color raster selection, so that mode alone does not qualify provoking-color behavior. Ten additional cases render original primitives and CPU-lowered/re-emitted primitives through the SAME actual GL context and compare nonzero RGB readbacks exactly, including flat colors. These are selected GL-versus-GL lowering tests, not SDL_GPU pixels or cross-driver image parity.

The selected 37-degree normalized-axis rotation/frustum trace measures each differing float component. A local arithmetic-test bound of 2^-18 absolute is used only for this fixed bounded trace. Actual measured errors are logged separately. This is not a gameplay, pixel, arbitrary-matrix or across-platform tolerance. The inherited VGUI sequence is characterized exactly: it pushes modelview twice, rewrites projection and pops modelview twice; projection remains rewritten.

Configure with explicit matching-config source-built SDL library and runtime paths:

```sh
cmake -S tests/native/render_state -B build/render-state-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_RENDER_SDL_LIBRARY=/absolute/path/to/libSDL3.0.dylib -DCSE_RENDER_SDL_RUNTIME=/absolute/path/to/libSDL3.0.dylib -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/render-state-release
ctest --test-dir build/render-state-release --output-on-failure -V
```

On Windows use a matching MSVC environment and `-DCSE_TARGET_ARCH=x64` (or x86/arm64), an SDL3 import library for CSE_RENDER_SDL_LIBRARY and SDL3.dll for CSE_RENDER_SDL_RUNTIME. All GL calls are loaded through SDL_GL_GetProcAddress with their actual calling convention. The oracle needs a real driver supporting the three-unit engine profile; missing context/entrypoints/features fail explicitly. `-DCSE_RENDER_GL_ORACLE=OFF` builds the independent CPU suite, and `-DCSE_RENDER_SANITIZERS=ON` enables Clang ASan/UBSan. C++14 and explicit Release O2 / Debug O0 or Od, warnings-as-errors are enforced.

Observed driver qualification: Apple M5 Max compatibility GL2.1 passes all selected exact RGB lowering comparisons. Interactive Parallels/Apple GL4.3 reports LAST_VERTEX_CONVENTION but the same flat comparisons fail on both x64 and Win32, with identical captured RGBs across those ISAs. Smooth comparisons and state/feedback queries pass. The fixture keeps the lowering test RED on that provider; it records requested/actual shade mode, provoking state, submission order, raw RGBs and differences. Canonical documented last-provoking behavior is retained in the CPU layer. No provider-specific workaround or tolerance is applied. Noninteractive VM-exec probes cannot load glActiveTexture; that separate failed-context receipt is retained.

## Owned texture capture

`legacy_texture.{h,cpp}` adds reserved/generated names, separate default objects,
per-unit 2D/rectangle bindings, target identity, deletion and name reuse. Raw names
retain all 32 bits. Immutable objects/images and shared palettes are owned by draw
snapshots; later subimages, parameters, deletion or recorder destruction cannot
change an existing draw. Copies share immutable versions and their physical byte
charge, then use copy-on-write. They own independent geometry/list containers and
logical current state. Fresh recorders have independent storage budgets. Pixel
storage remains charged until the last retaining draw/context releases it; the
bound includes transient replacement images. Counts bound names, levels, lists
and draws, while a separate quota bounds recorded upload bytes.

The selected unsigned-byte profile includes RGB/RGBA/BGRA, luminance, alpha,
luminance-alpha and palette-index sources; explicit mip levels, subimages,
unpack alignment/row length and pack alignment. It owns tight logical RGBA
sampling components or original indices, retaining the requested sized/unsized
internal format. RGB5_A1/RGBA4 and unsized storage conversion is left to the
backend. No CPU quantization or hardware precision parity is claimed. Null image
allocation records undefined content; its zeroed backing vector is not a promise
of initialized GPU texels. Texture completeness/filtering, anisotropy capability
clamping, border sampling and actual GPU sampler behavior remain backend work.
The distinct legacy CLAMP and CLAMP_TO_EDGE requests are retained.

Sampler state belongs to the texture object. Environment MODULATE/REPLACE and the
selected default COMBINE_RGB=MODULATE plus RGB_SCALE=2 detail path are per unit.
The selected SDK also submits GL_ALPHA as an environment mode; actual GL rejects
it and preserves the previous mode. The fixture characterizes that result.
PixelStore, GenerateTextures and DeleteTextures execute immediately during list
compilation. Upload lists own their bytes and capture-time unpack state; geometry
list replay resolves the then-current texture objects and environment.

`EngineTextureCache` models the separate two-unit engine software cache, rather
than masking every raw GL texture name. A packed handle uses low16 as the name
and positive upper16 minus one as the palette selection. A same-low16 hit returns
before palette selection, preserving the owning GL_Bind behavior. Raw binds and
the third detail unit do not update that cache. The shared RGB palette remains
global and versioned independently of texture objects. Apple GL exposes no
GL_EXT_shared_texture_palette here: palette/index lifetime and packed handles
have CPU evidence, with no extension raster acceptance claim.

The texture oracle compares exact selected upload/readback bytes and four
original-GL versus owned-snapshot-GL raster profiles: replacement, fog/blend/
scissor, three-unit detail scaling, and linear RGB5_A1 conversion through the
same driver. Each captures 12,288 RGB bytes, deletes original texture objects,
then replays retained versions. This qualifies capture/re-emission for those
cases, not a CPU rasterizer, SDL_GPU pixels or general rendering equivalence.
Readback uses the separate query component mapping from
[OpenGL 2.1 table 6.1](https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf#page=267);
it does not replace the sampling expansion. The initial incorrect query
expectation is retained as a RED receipt.

## Resource count comparison

`operation_corpus.py` evaluates the actual owning hw manifest with the retained
translator and records selected-source hashes, lexical calls and raw GL bypasses.
It does not preprocess branches or claim runtime coverage. The installed
`platform/gl_shaders/vs_world.vert` and `fs_world.frag` were copied read-only to
the external `renderer-asset-reference` artifact with source paths and hashes;
their text is not redistributed in this repository or treated as owned source.

| Resource | Selected source/asset requirement | Qualified SDL source profile |
| --- | --- | --- |
| Fragment sampled textures | At most 3 per world path; 2D and rectangle snapshots retained separately | 16 samplers per stage |
| Vertex sampled textures | 0 in the referenced world vertex shader | 16 samplers per stage |
| Storage resources | 0 in the selected fixed-function/world paths | D3D12 fewer-slots profile: 8 per stage |
| Uniform blocks | Not implemented/reflected; 5 matrices occupy 320 bytes, fog/environment constants are small | 4 blocks per stage |

These are count-compatible with the measured Parallels D3D12 DXBC/fewer-slots
and Metal paths. Actual renderer offline shaders, reflected layouts and pipelines
are still required. The earlier GPU proof used SDL's blit shaders only. No DXIL,
Metal compiler, runtime MSL fallback, default backend or production dispatch is
introduced here. The existing world shader's exact overbright/detail/fog math is
reference evidence for the later shader stage, not newly implemented behavior.

Raw GL calls include VGUI's unpack-row-length reset, WebM texture/pixel-store/
draw operations, fog/list calls, and the framebuffer/presentation route. No
callsite has been hooked, so neither raw calls nor qgl calls reach this CPU layer
in production. Shader selection, framebuffer/MSAA/resolve, clears, screenshots,
presentation, driver precision/filtering and all GPU execution remain open.

## Owned ordered CPU frame capture

`FrameRecorder` wraps the persistent legacy Recorder with an explicit frame
boundary. A sealed `FrameSnapshot` owns draw geometry, immutable texture versions,
and the ordered Draw/Clear/Readback stream; it contains no callback, client pointer,
GL/SDL object or native handle. List replay contributes each draw in order. Clear
records explicit private mask bits, write masks and scissor state. Readback records
only an ID, validated stage and contained GL bottom-left pixel rectangle; no GPU
readback occurs here.

Frames require a nonzero serial/display epoch and positive dimensions bounded by
`kTextureDimension`. Commands and forwarding methods require an active frame.
The first command/validation/allocation failure poisons the frame; Seal preserves
its output on failure. Successful Seal requires empty output vectors and moves
ownership without resetting persistent state, texture objects or completed lists.
An empty frame is valid. Event count is independently bounded from draw/geometry
and texture quotas. Accepted nonzero readback IDs increase strictly across seals
and Abort; failed appends do not consume IDs and exhaustion requires a fresh
recorder epoch.

Abort discards draws/events and unfinished primitives/list compilation without
allocating. It releases pending list/upload/vertex ownership while retaining
completed lists, submitted state/texture mutations and the accepted ID watermark.
Allocation exceptions are caught only at this private FrameRecorder boundary and
reported as `LimitExceeded`; constructor failure propagates `std::bad_alloc` to
its owner. The standalone legacy Recorder retains its existing exception policy.

The private `render_frame_tests` executable registers `render-frame-order`,
`render-frame-owned`, `render-frame-boundaries`, `render-frame-limits`,
`render-frame-abort`, `render-frame-persistent`, and
`render-frame-allocation-failure`. The private CPU library and frame fixture
compile with `CSE_CAPTURE_FAULT_INJECTION=1`; an RAII fixture hook can fail the Nth
named ownership allocation. Existing state/texture fixtures leave the borrowed
thread-local hook unset. Production manifests do not define this switch, and its
checkpoints compile to no-ops outside the fixture. The allocation case reports
actual named hits and tests later-hit cleanup, sticky failure, allocation-free
Abort, retained snapshots and recovery. The ownership case requires actual source
pixel/vertex addresses above `UINT32_MAX` on 64-bit lanes; unavailable placement
is an explicit failed/unqualified case, never a skip. Win32 omits that address
requirement while retaining the ownership assertions.

CPU-only configuration (from an initialized matching MSVC environment on Windows):

```sh
cmake -S tests/native/render_state -B build/render-frame-x64-debug -G Ninja -DCSE_RENDER_GL_ORACLE=OFF -DCSE_TARGET_ARCH=x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/render-frame-x64-debug --target render_frame_tests render_state_tests render_texture_tests --parallel 1
ctest --test-dir build/render-frame-x64-debug --output-on-failure --parallel 1
```

Use a separate Release directory and `CMAKE_BUILD_TYPE=Release`. Qualification
order is actual Windows x64 Debug/Release first, then Mac ARM64 Debug/Release with
`CSE_TARGET_ARCH=arm64`, `CMAKE_OSX_ARCHITECTURES=arm64`, and
`CMAKE_OSX_DEPLOYMENT_TARGET=14.0`, followed by the Clang CPU sanitizer variant.
The missing-member compile RED and linked missing-Draw-event behavior RED are
preserved as separate external evidence. This source change does not claim full
renderer, backend, game, pixel equivalence or migration acceptance.
