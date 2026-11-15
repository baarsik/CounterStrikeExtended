# VGUI backdrop fixtures

Configure from the repository root with the matching native SDL3 library:

```sh
cmake -S tests/native/vgui_backdrop -B build/vgui-backdrop -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCSE_BACKDROP_SDL_LIBRARY=/absolute/path/to/the/native/SDL3/shared-library
cmake --build build/vgui-backdrop --parallel
ctest --test-dir build/vgui-backdrop --output-on-failure
```

`vgui-backdrop-real-gl` uses a real hidden SDL/OpenGL compatibility window. It checks the production
renderer against high-frequency scene pixels: matched darkness/blur fades, transparent edges, parent
clipping, a marker drawn after capture, sharp foreground painting, demand, resize and shutdown/reuse.
Additional cases cover zero framebuffer alpha, arbitrary blend equations, texture units/matrices,
secondary color and clip planes, separate read/draw framebuffer bindings and multisampled sources.
No framebuffer readback is used by production rendering; the fixture reads pixels for its assertions.

`vgui-backdrop-shared-bindings` and `vgui-backdrop-cs-bindings` compile the actual controls lifecycle
functions extracted from each module and exercise initialization/shutdown against a minimal interface
factory. The public optional-service query and binding implementation are production code. Unrelated
GUI providers are substituted so the test does not need to initialize fonts or the complete game.
This catches a module that draws the gradient fallback because it never bound the blur service.

These tests do not establish in-game appearance, gameplay performance, interactive settings layout
or Windows driver compatibility. Owning runtime builds and gameplay checks remain separate.

The style policy fixture pins saved style values, optional API validation, palette ownership, rounded
geometry and compact spectator rail bounds. Rounded real-GL cases verify that the silhouette stays in
its original coordinates under parent clipping. The legacy style fixture compiles the actual VGUI1
background owner, material helper and HL scoreboard hover painter; local-player/team, fading killer and
mute-hover RGB/inverse-alpha survive styling, while literal foreground colors stay producer-owned.

The native Options fixture in `tests/native/server_browser_ui` also exercises Original ↔ Smoked Tactical
Glass through the real settings page and shared controls under ASan/UBSan. It checks the renamed option,
retained open-page/selection/input, absence of Original material requests, restoration of original colors,
never-painted hidden controls and parent-owned playback icons/child transparency. Its SDL renderer checks
the no-GL fallback; the separate real-GL fixture checks blurred composition.
