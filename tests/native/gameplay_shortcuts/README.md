# Owned Cocoa gameplay shortcut policy

The fixture compiles the complete production `sdl3_native_cocoa_mouse.mm` and
injects named typed SDL/AppKit operations into the new bodies. It tests exact
result/guard ordering, genuine SDL/native identity, monitor transactions,
failures/exceptions, nested admission/removal/filtering, generation invalidation,
and refusal to authorize retirement while a callback remains on the stack.
The fixtures neither generate native input nor prove physical gameplay behavior.

The unchanged `native_mouse_window/cocoa_tests.mm` and
`system_capture/interaction_tests.mm` run against that same complete owning TU.
Unused default SDL lookup seams abort if accidentally reached. No older provider
service or entrypoint is replaced with a model.

Configure with pinned SDL public headers and its retained generated build config:

```sh
cmake -S tests/native/gameplay_shortcuts -B <fresh-build> -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_SDL_INCLUDE=<retained-cache>/dependencies/SDL3/source/SDL3-3.4.20/include \
  -DCSE_SDL_CONFIG=<retained-cache>/dependencies/SDL3/upstream/include-config-release/build_config
cmake --build <fresh-build> --parallel 2
ctest --test-dir <fresh-build> --output-on-failure
```

Repeat Debug and Debug with `-DCSE_SHORTCUT_SANITIZERS=ON`. The standalone suite
is the runnable test project for this additive change; the repository has many
independent native fixture projects rather than one all-platform native suite.

`api_presence.py <exact-provider>` distinguishes a missing new symbol from a
behavioral failure. Its baseline exit17 is **API-absence RED**, not proof that a
new keyboard regression was observed. The controller's native baseline probes
establish actual Control-Space layout switching and that StopTextInput alone
does not prevent it.

`build_provider_packet.py` recompiles exactly the owning TU using a retained
Release/Debug command, then full-links the matching SDL image with every other
object/dependency and upstream archive/source pinned unchanged. It writes a new
packet, signs local artifacts ad hoc, records `nm`/Mach-O/signature identities,
and builds but does not launch the video-only exported-policy probe. It does not
configure/mutate the retained cache, stage a runtime, or initialize audio.

The probe accepts `--enabled`, `--relative`, and `--text-off`. F1 toggles the
exported policy, F2 toggles SDL text input, Escape exits after successful explicit
removal. It prints unchanged SDL key events for controller observation. Native
focus, layout preservation and actual down/up sequence require the controller's
OS qualification; a successful component suite is not that evidence.
