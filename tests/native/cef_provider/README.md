# Native CEF provider fixture

This standalone fixture uses the pinned CEF distribution, stable API 15600 and
sandboxed native processes. It does not load the game or implement its HTML
controller. Nothing executes as part of the build.

Configure with Ninja, an architecture-matched compiler and
`-DCEF_ROOT=<extracted pinned distribution> -DCMAKE_BUILD_TYPE=Release` (or Debug).
Only the pinned Windows x64 and macOS ARM64 distributions are accepted. The CEF
wrapper compiles as C++20 independently of game targets. macOS apps and all five
helpers target macOS 14 and use programmatic Cocoa startup without XIB resources.
The Windows app uses CEF's sandbox bootstrap and owned entry-point DLL.

After the applicable runtime authorization, run:

```text
python run_fixture.py --executable <built executable> --evidence <new absolute evidence directory>
```

The driver owns an ephemeral loopback HTTP server and a new writable profile.
Two independent browser processes exercise exact BGRA pixels at two sizes,
mouse clicks, UTF-16 character events including a supplementary character,
JavaScript, persistent cookies/local storage and the persistent HTTP cache.
Browser close callbacks, cookie flushing, helper termination and bounded shutdown
are mandatory. Failures retain process output, state and the isolated profile.

A passing fixture establishes this SDK/provider boundary only. Game MOTD/UI
integration, full serializer/callback behavior, media and renderer integration
have separate acceptance gates.
