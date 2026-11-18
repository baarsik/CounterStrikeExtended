# Software platform owner fixtures

Run `python3 tests/native/software_video/run_tests.py <configured-swds-build>` after building the full `swds` graph. The runner derives the include paths, target macros and numerical policy from that build's `vid_win.cpp` compile command. It removes the forced Steam initializer only from controlled component fixtures; actual image tests load the actual owned Steam provider.

The required cases are:

- `software_checked_allocations`: the actual `vid_win.cpp` owner rejects zero, wrapped-negative, over-height/width and oversized cache requests; accepts 5120×1600; preserves high-hunk depth/cache partitioning. The actual exported software cache-size method rejects malformed/overflow arguments. The actual `Memory_Init`/studio loader/cache/model owner tests reset model cache references across a new heap generation.
- `software_snapshot_rows` exercises writes from `VID_TakeSnapshot` through the `GAMECONFIG` path. The fixture decodes the lossless WebP and checks odd-width row order, padded source pitch, 555/565 color quantization, and a 5120-pixel row.
- `software_pixels_and_palette`: the actual VID palette retains all 256×4 unsigned-short storage; the actual image's retained `Draw_FillRGB` writes 555/565 CPU pixels, which the production presenter uploads and reads back through the real SDL dummy driver.
- `software_video_lifecycle`: real presenter texture upload/readback, stale SDL renderer generation, idempotent release and restore; controlled texture creation/configuration failure; the actual `IVideoMode` concrete owner handles partial window/renderer creation and maintains framebuffer aliases/pitches.
- `dedicated_load_without_video` uses the actual software image and binds the executable-owned socket and software lifetimes. It checks both factories, unavailable launcher/filesystem rejection, retry, repeated shutdown, version checks, active-session rejection, and terminal retirement before unloading. A preloaded test library aborts SDL video initialization, cursor or window construction, and GL creation or attribute calls throughout the process.

`host_shutdown_fixture.cpp` substitutes subsystem boundaries around the verbatim current `host.cpp` prefix and `Host_Shutdown` definition. It checks drain → Steam → network → game DLL ordering, recursive and repeated calls, and staged GameUI/VGUI completion for the listen server.

For owned game/data cohorts, run the generated `dedicated_load_without_video` from an isolated runtime directory, with `DYLD_LIBRARY_PATH` set to that directory and `DYLD_INSERT_LIBRARIES` set to the generated `forbid_video.dylib`. Arguments are the software image, owned filesystem image, absolute data root, game (`cstrike` or `valve`), map and optional local port. It binds via `+ip 127.0.0.1`, runs frames/status/map, changelevel/status, shutdown, valid reinit/status and normal teardown. The process exit is part of the result; a printed PASS followed by a teardown crash is a failure. No client presentation, VGUI admission, authentication exemption or external server compatibility is implied by these headless checks.

`studio_cache_tests.cpp` is an additional diagnostic that runs the actual studio loader with a guarded allocation service. `studio_heap_tests.cpp` accepts optional owned `.mdl` paths after the image argument; it exercises the actual cache allocator and high-address hunk relocation and then the model-generation reset. Asset data is not included in this repository.

`run_dedicated_owner_test.py <output-directory>` extracts the current production dedicated interfaces, class/state and Init/Shutdown/Retire/RunFrame methods verbatim. Controlled external engine/game/filesystem/registry/argument cleanup callbacks reenter Shutdown, Init and Retire. It verifies no nested dependent cleanup, rejection until the outer call returns, game-cleanup failure and engine exception retry, double shutdown, successful restart and terminal retirement. The normal runner includes this fixture.

Task2 also runs `software_surface_tests` against the real finished software image.
It rejects unrelated and actual hardware backend instances, uploads high-address
font atlas buffers, draws exact padded 555/565 glyph samples, retires bound pages,
and checks native BGRA image conversion, region updates and invalid-region rejection.
`tests/native/mac_client` adds the complete owned software font pipeline: real
Helvetica glyphs through selection, atlas upload and SW drawing, pen advancement,
active-scope teardown refusal, renderer recreation and quiescent root close.
These checks do not call SteamAPI_Init or replace the required two-game live checks.

`python3 tests/native/software_video/run_surface_tests.py <configured-swds-build>`
runs the no-login actual image surface cases independently. Besides VGUI1 atlas
samples, it executes the real VGUI2 `FontManager` / `FontTextureCache` /
`BaseUISurface::DrawUnicodeChar` chain with actual FreeType glyphs, verifies zero
alpha pixels preserve the framebuffer, and compares generic image/global-alpha/
additive output against direct retained raster calls in both555/565 formats.
Dirty BGRA updates use full-frame source pitch and nonzero multirow rectangles.
The separate atlas retirement suite also compiles the exact Win32 branch for
bound BGRA replacement; this is branch coverage, not a Windows x86 build claim.
