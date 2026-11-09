# Pure actual engine font-atlas methods

This fixture uses complete checked-in EngineSurfaceWrap, SurfaceBase, Font and IEngineSurface declarations. A lexical
extractor pins/copies the exact production FontInfoVGUI/state and nine atlas/draw/forwarding methods. The whole actual
Font.cpp (getTextSize/getId/getPlat) compiles unchanged. It does not compile or execute the whole engine TU at runtime.
Separate owning-TU compilation checks do not execute it.

Authored member constructors initialize the real complete base/member objects; no vptr forging, copied class layout,
placement into unconstructed storage or OS constructor is used. ModelFont derives from real Font and supplies typed,
controlled metric/raster/ID virtuals. Those are authored data, not a font provider or readiness stub. Unrelated virtual
methods abort. All objects remain alive during use. A recording complete IEngineSurface interface owns copies of each
512x512 upload and all bind/quad/pen commands. DarStorage uses legal protected member pointers solely to reclaim fixture
storage; production constructors, Dar/cache allocation lifetime and shutdown are not fixed or qualified by this glue.

Modes: --admission safely counts glyph calls without drawing unwritten UVs; --width draws only printable WiA;
--core checks all256 admission BEFORE any high-byte UV use, ABC/measurement/endpen, exact power-of-two UVs, full upload
pixels/padding, row/page boundaries, zero width, 511/512 edges, bearings, unsigned draw bytes, null/zero/negative lengths,
NUL/C-string distinctions, cache reuse, distinct miss IDs and explicitly simulated stale generation rebuilding.
Four initialized atlas pages are the maximum core domain. --capacity-diagnostic exits78 after the fourth upload before
entering uninitialized bind slot4; it does not read a bad slot or run deliberate out-of-bounds code.

Inherited behavior remains explicit: distinct fonts with cache-hit ID-1 collide under ID-only keys; invalidate is
empty; only four of eight bind slots are initialized; page8 exhaustion aliases storage in the final slot calculation.
The collision is a diagnostic, not desired font identity behavior. No eight-page/exhaustion safety is claimed. A test
counter increment is not an actual renderer/context-loss event or lease implementation.

Initial source REDs are256 expected vs95 C-locale admissions and WiA width21 measured vs50 drawn. The approved production
patch changes only three selector directives: native64 follows existing Windows B-width/all-byte behavior; older32POSIX
keeps maximum-width/isprint behavior. Windows selected tokens remain unchanged. Constructor/cache/page/lifetime code is
unchanged. Generated mutants perturb atlas width, draw width, admission or UV scale; no production mutant is written.

Mac fixture source is C++14/NoEH/no-fast-math/no-contraction; MSVC is C++14/NoEH/fast FP. Run Release/Debug/sanitizers and the six MSVC CPU/
configuration contexts through the packet runners. Real SDL/App/font/GPU/provider/window/UI/registration never runs.
PublisherReady/SurfaceReady/ColdLoadReady remain false. The FixedSys V2 contact sheets and their unaccepted readability
remain separate and immutable. This mechanical atlas gate cannot qualify glyph readability, real fonts or whole UI.
