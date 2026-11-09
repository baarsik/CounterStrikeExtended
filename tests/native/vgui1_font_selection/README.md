This standalone C++14 component is unselected. It compiles actual Scheme, Font,
Cursor, native Font and FontServices sources, and calls their real methods. The
only resource provider is `fake_provider.cpp`: registration touches no file,
system font, window or host UI; its controlled face supplies metrics and pixels.
The unrelated default-cursor constructor is also controlled. There is no draw
binding: the fixture's Surface selection symbol aborts if called.

`Selection` holds only copied platform/numeric-ID/catalog/renderer identities,
plus required root callback context and a module lease token. It retains no
Font or Scheme facade. `BeginUse` holds a required root whole-use guard across
real Prepare and the caller's later metric/raster/atlas work until EndUse. The
four default Scheme slots are read only under that same required guard. Ready
is a prepared snapshot, never Surface readiness. Callers still BeginUse for each
actual use; MarkAtlasReady only acknowledges the renderer's own completed work.

The callback owner in these tests is a model. Production integration is
incomplete until one real root owner pins module/code/records and excludes
EVERY ReleaseFiles/rebind/unload mutation against every whole-use guard. Nested
guards are required for four-font admission. A failed Unpin retains the entire
lease and must prohibit unload/rebind success until retry. Context and callback
code must outlive all leases. Calls on a Selection must be externally serialized;
these tests check wrong-thread rejection without racing state mutations. No
late static-destructor callback is permitted; destroying a held lease aborts.

Run `run_mac_matrix.py --art ABSOLUTE_DIRECTORY` for actual-owner Release,
Debug and ASan/UBSan execution in private and real public caller-header views.
Actual owner TUs always retain their private declaring headers. The public
view applies to the new component, caller and declaration probe only. This
avoids pretending Scheme.cpp's private NULL dependency is a public-owner context.

`run_negative_controls.py` builds isolated copies and requires eight deliberately
broken behaviors to fail. It never edits production source. The original
missing-source RED and the reentrant in-progress-pin retirement RED are retained
in the evidence packet. `run_windows_compile_matrix.py` uses real MSVC x86/x64/
ARM64 Release/Debug profiles for 24 compile-only actual-TU/header probes. Native
Font/FontServices stay Apple-only; this is not Windows font-owner execution.

Important limits: no real resource/font provider, renderer atlas, root module
lease implementation, Surface, App integration, loader/publisher, static record
or cold-load reader is selected or qualified. PublisherReady, SurfaceReady and
ColdLoadReady are false. The uint64 use-serial exhaustion return exists but is
not reached dynamically by the bounded normal-state fixture.
