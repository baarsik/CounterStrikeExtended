# Unselected native VGUI1 provider component

Four new private files implement an indexed owned SFNT face and the existing provider table. There is no production qualified factory, manifest selection, public layout change, Surface binding, or FixedSys alias. The default Apple services reuse the pinned discovery helper and the single shared registration service; automatic tests inject authored data/services and never call those native entry points.

The native face deliberately retains interpreter 40, Harmony geometry 21, the frozen ordinary candidate's native metric conversion, persistent canvas/call-order behavior, legacy BGR/weighted alpha and nonzero pen offsets. New tests compare every ordinary cell against that frozen candidate, not against a claimed GDI-equivalent backend. Intrinsic weight0/400/700, explicit symbol cmap enumeration and collection indices are distinct tests. Unsupported width/rotation/effects/weights/charset and ambiguous cmap mappings fail ordinarily.

The old owner is compiled unchanged in the Apple-only integration test with a fixture-only qualified link binding and injected services. Bitmap input takes precedence, malformed present bitmap does not substitute, retired requests use Prepare, and failure/retry/release/rebind keep ownership semantics. The standalone fixture has no Surface; calling its fail-fast binding aborts.

Sources need C++14, NoEH and strict FP for native face/legacy transfer. Provider uses C++17 to consume the existing helper headers. Bookkeeping allocation failure follows the owner's inherited NoEH fatal policy. A context cannot be destroyed while registered owner tokens remain; ReleaseFailed requires root to retain the context/module and refuse unload/rebind. Calls are serialized by the owner; independent thread-safe face use is not claimed. No production deadline, forced release or unqualified capability is supplied.

Configure with CSE_FREETYPE_PREFIX pointing at exact source-built 2.14.3 and macOS ARM64/min14 flags; run CTest. CSE_PROVIDER_SANITIZERS instruments owned source; dependencies remain pinned. Windows matrix compiles/runs only two portable authored/injected tests, not a native Apple provider.

Authored inputs are generated from the previous plain-geometry SFNT generator, never a host font. Variants include intrinsic bold, actual direct/high/equivalent/ambiguous MS symbol cmaps and a two-face TTC. No automatic target executes real discovery, font registration or game-resource reads. Separate real-resource commands require reviewed authorization after binaries are sealed.
