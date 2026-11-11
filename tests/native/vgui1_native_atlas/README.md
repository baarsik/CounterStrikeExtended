This unselected C++14 CPU component exercises real FontServices, native Font,
common Font, NativeFontSelection and new Runtime/Atlas code in two independently
loadable dylibs. The host uses the existing owned native_module facility and
retains an external root reference through last internal release. Actual Open
and Selection pins use real LoadModule references to both exact root and actual
FontQuery provider images; counters only track those genuine handles and uses.
The process host supplies distinct module epochs. No engine-local process epoch,
ABI reader, native provider factory or readiness promotion is invented.

The fake font provider is fixture-only guarded source. Registration accepts
controlled strings, no filesystem or system font call. Raster bytes are authored
pixels. Complete actual Scheme/Cursor code links for NativeFontSelection symbols;
its unrelated default cursor constructor is controlled and never used by this
fixture. No default Scheme, App, Surface or window is created. Real public/private
Font classes retain their actual declarations; no surrogate object or Font field
cast is used. The actual private BaseFontPlat interface is dereferenced only
inside a successful BeginUse. Separate provider/consumer typed probes assert every
private method signature and compare physical scalar/record facts. Compiler
vtable/layout/assembly observations remain separate from a complete ABI catalog.

The renderer is controlled owned memory: complete 512x512 RGBA uploads, texture
ownership and checked quads. Tests cover all256 byte admission, per-B geometry,
ID-1 collisions, same-platform cache sharing, facade deletion, nested genuine
leases/scopes/reentry, unchanged-catalog release failure, retained-state retry,
normal/lost/changed epochs, loss during upload,8 complete pages,9th-page rejection,
upload rollback and failed retirement. One active atlas is retained per surface;
a second entry exists only during checked replacement/rollback. No historical
Font pointer cache exists. Null/no-font and nonpositive-length no-ops preserve the
legacy method contract; they are not font readiness results.

The HW/SW retirement executables extract exact private source bodies and metadata
owner declarations. HW uses actual CUtlRBTree storage and injected GL/SDL lookup
entrypoints, with no SDL/GL linkage. SW constructs the complete actual EngineSurface
with its actual constructor and private render-plat methods; typed unrelated
virtual bodies abort. It owns real copied pixel arrays and tests unlink/binding/
array release. Its inherited render-plat allocation is cleaned up ONLY by the
fixture after the actual destructor; this is not full production SW ownership
qualification. Legacy Remove/FreeTextures array deletion outside the new native
font-retirement path remains outside this patch. Private successful name retirement
is one-shot owned input; Atlas clears each slot and makes public repeated retirement
idempotent. This does not accept arbitrary or double private-name retirement.

Run `run_macos.py --art NEW_DIRECTORY --before PINNED_PRODUCTION_BEFORE_DIRECTORY`
for the Mac matrix and existing/derived owner compiles. Actual whole changed TUs
use existing hw/swds compiler recipes before/after. New private files compile under
explicitly derived versions of those recipes; manifests are NOT edited. The core
fixture uses C++14/NoEH/precise FP. Retirement targets use C++17 because unchanged
real metadata static_assert declarations require it; no diagnostic suppression or
rewritten production declaration is used. Sanitizers cover a single actual unload;
uninstrumented Release/Debug also cover repeated real unload/reload. An optional
instrumented reload diagnostic is recorded separately and never suppressed.

Native branches in actual engine/window owners are compile evidence only here.
No real App/Surface bootstrap, provider, font discovery/registration, SDL device,
GPU, renderer pixels or whole UI runs occur. AppCold/App services/Surface/readability
flags remain false. Production bootstrap must supply a checked, nonreused epoch /
image binding, checked default and retired-font preparation, and consume shutdown
status before legacy void entrypoints. Whole-game/UI/layout acceptance stays open.

The Runtime V1 and V2 binding cases each run with a real inert
DYLD_INSERT_LIBRARIES image. Both retain missing-route, foreign-dylib callback,
wrong-image and zero-epoch rejection before the full real-loader suite. The
inserted host verifies that the inert image loaded and reports whether dyld
enumeration index zero names the genuine executable. Runtime admission uses the
public `_NSGetMachExecuteHeader` identity for both callback versions.
