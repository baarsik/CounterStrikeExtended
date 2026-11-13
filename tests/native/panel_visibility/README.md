This fixture extracts the current `Panel::SetVisible`, `VPanel::SetVisible`,
`VPanel::SetParent`, private notification traversal, and HTML resize/notification
methods without rewriting their bodies. Only the VGUI services, hierarchy
storage, and browser serializer are controlled. Producing source and body hashes
are generated beside the executable. It does not execute a browser or native UI.

Both the shared HTML implementation and Counter-Strike's separate HTML implementation
run through this hierarchy fixture. Each target extracts its notification declaration
from the owning production header, so a missing CS handler cannot be supplied only
by the fixture. The shared panel hierarchy supplies the same VPanel notification
boundary to both implementations.

The initial implementation fails because showing a parent overwrites an HTML
child's own hidden flag. Native 64-bit builds now notify through the existing
client message boundary, including nested hidden descendants and direct IPanel
changes. HTML sends zero dimensions to suspend its owned provider, releases
provider focus, and restores dimensions/scroll state when effectively visible.
The private notification does not dispatch a user command or add a virtual slot.
The maintained Win32 reference branch retains its original implementation.

The controlled tests cover nested ancestry, independently hidden children,
direct low-level visibility, reparenting/detaching, hidden resize, restored
scroll state, unchanged-size caching, and a missing serializer. Compilation of
the real owning translation units is a separate gate. Whole VGUI/CEF rendering,
real input focus, asynchronous provider readiness, and game lifecycle acceptance
remain separate from this fixture.

Configure with CMake/Ninja using Release or Debug. Set
`CSE_VISIBILITY_SANITIZERS=ON` for the native sanitizer run. MSVC execution is
intended for x64 and ARM64; Win32 is the unchanged reference implementation.
