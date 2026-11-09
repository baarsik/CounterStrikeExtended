This initial Apple component is deliberately unselected. It owns the existing
VGUI1 Font entry points, a distinct private platform interface, copied requests,
bitmap fonts and versioned bind/preflight/query/prepare/release state. It changes no
public Font field, slot or Windows/Linux source selection. Source selection must
replace the Windows Font TU, retain the common Font/fileimage/vfontdata TUs and
provide both required private bindings before it can link.

The component fixture supplies a fake provider that reads authored identity
model files and returns deterministic metrics/bytes. It links no CoreText,
FreeType or system-font service and proves forwarding/ownership/status only.
The bitmap path does use the unchanged production TGA decoder with authored
data. Native GDI-equivalent rasterization, exact font-program availability,
width/rotation/style mathematics and actual consumer atlas/layout remain open.
The separate real_gdi packet measures real Windows memory GDI and does not
qualify these fake native outputs.

Integration contract:

- Bind complete VFS-resolved UTF8 paths, then check preflight before exposing
  metrics. FilesBound is not Ready. Pending/unavailable is an ordinary diagnostic,
  not successful metrics or an arbitrary substitute family. An early empty
  preflight is insufficient: requests created after binding are checked during
  construction; constructors that immediately use metrics require a successful
  bound/provider state beforehand. Bootstrap must establish this earlier boundary.
- Global preflight checks only the current lookup generation. Query is passive.
  Before legacy metric/raster use, bootstrap must call CseVgui1FontPrepareV1 for
  retained defaultScheme fonts and active borrowers outside that generation,
  including initially pending requests and formerly Ready requests after file
  release/rebind. Pass Font::getPlat() and the versioned status; missing resources
  remain ordinary Pending/Unavailable results. Prepare neither reactivates retired
  lookup entries nor replaces Ready faces. It retries an unavailable request only
  after a new catalog is bound. A bad argument/foreign platform leaves status
  unchanged; platform membership is checked without dereferencing the argument.
- Additive binding and lookup reset retain existing Ready faces and public
  borrowers. New generations retry failed requests. Reset retires lookup only;
  the inherited cached ID-1 behavior is characterized rather than corrected.
- The provider must implement every requested capability and the explicit
  resolution profile. Three exact measured GDI fallback/charset tuples are
  pinned; other requests carry their exact identity. These are not general
  family substitutions. ANSI byte profile is explicit CP1252; SYMBOL is separate.
- Stop all metric/raster users before release. Faces close before file-owner
  release and context destruction. ReleaseFailed retains context/owner tokens;
  root must retain the module, report failure and refuse rebind/unload. Explicit
  retry may be bounded. Permanent-failure unload is unsupported. Process exit
  may leave process-scoped registrations for OS reclamation. There is no forced
  unload, detach, destructor abort or claim that this service can veto dlclose.
  Prepare returns ReleaseFailed while release is pending, without realization,
  even when its reported per-font state is still Ready after a failed rollback.
- Only unexpected use of an unready old API aborts with a diagnostic. Ordinary
  missing fonts are reported by checked preflight/prepare and passive Query. Bookkeeping follows the
  inherited NoEH policy: allocation exhaustion is process-fatal. No provider may
  throw across a noexcept service boundary.
- System glyphs accept only the explicit byte domain (-128..255); sign-extended
  bytes are normalized, rather than indexing a negative Windows array. This is
  a defined native safety boundary, not parity for inherited invalid indexing.
  Bitmap negative ABC clamps to zero and high ABC clamps to255 as the reference;
  invalid bitmap raster indexes/canvases fail closed. Truncated/unrepresentable
  bitmap inputs report failure before publishing a decoded font.

The actual component is tested through two independently unloadable consumer
dylibs, failed release with the module retained, explicit successful retry,
repeat unload/reload and process-exit destructors that use live metrics before
service teardown. No permanent-failure unload is executed or promoted as safe.
The pinned Apple sanitizer fails during registration on repeated instrumented
loads. The initial candidate retains its complete failing case. An independent
leaf with one out-of-line virtual class and two tiny dependent clients also
fails in __asan_register_globals on its second load without using this component
or font APIs; the plain int-array leaf passes. This supports a qualification
split for this measured toolchain/case, not a claim about all Darwin loaders.
The final runner keeps both fresh failing stacks, runs instrumented API/single
unload/exit cases, and runs repeated component and independent reloads with the
uninstrumented profiles. It changes no sanitizer diagnostics or suppressions.
Leak checking is separate from address/undefined-behavior instrumentation.
