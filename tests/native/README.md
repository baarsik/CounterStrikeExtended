# Native component fixtures

Subdirectories are independent CMake projects. Where present, their README
describes production bodies, controlled dependencies, covered inputs and
coverage limits. The options below cover the simpler projects that have no
README. These fixtures check individual components; full runtime builds are
described in the [build reference](../../docs/building.md).

The portable bundled-translation checks live in `tests/localization_resources`:

```sh
cmake -S tests/localization_resources -B build/localization-resources -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/localization-resources --parallel
ctest --test-dir build/localization-resources --output-on-failure
```

On macOS, configure with `-DCSE_LOCALIZATION_RUNTIME_ROOT=/absolute/runtime/root`
to also exercise the actual VGUI/filesystem loader for every supported language,
the Korean alias, stock catalogue families and English fallback. This uses the
existing runtime libraries with dummy SDL providers; it does not launch a game.
With a C++ compiler, `-DCSE_LOCALIZATION_NATIVE_TESTS=ON` also checks the portable
disk-text decoder (UTF-16 endian variants, surrogate pairs, BOMs and legacy bytes).
The real-loader option enables that decoder check automatically.

The [legacy text consumer fixtures](localization_text/README.md) run the actual
title-message initializer/parser against all bundled title files and the
[team-menu fixture](team_menu/README.md) checks every bundled map briefing.

Russian/Polish validation additionally covers [actual font/layout fit](../localization_resources/README.md),
[HL HUD Unicode](hl_message_utf8/README.md), [HL menu bounds and fit](hl_menu_localization/README.md), [VGUI1 text and bitmap fallback](vgui1_utf8/README.md),
and [shared startup language selection](interface_language/README.md).

The Interface language selector has [actual Options control checks](server_browser_ui/README.md#interface-options-page)
and [strict native preference reader checks](interface_preferences/README.md).

For a pure component, from the repository root:

```sh
cmake -S tests/native/numerics -B build/numerics-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/numerics-release --parallel
ctest --test-dir build/numerics-release --output-on-failure
```

Use a fresh Debug directory with `-DCMAKE_BUILD_TYPE=Debug`. On Windows initialize
the matching MSVC target and set `-DCSE_TARGET_ARCH=x86`, `x64` or `arm64` where the
fixture supports it. On Apple Silicon use `-DCMAKE_OSX_ARCHITECTURES=arm64` and
`-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`. Sanitizer switches and required provider paths
are fixture-specific; read that fixture before execution. Real provider, device,
window and network probes are separate authorized actions, not implied by a build.

| Area | Starting points |
| --- | --- |
| CPU, counts and floating point | [CPU](cpu/), [numerics](numerics/), [surface lighting](surface_lighting/), [A3D FP state](a3d_float_state/README.md) |
| ABI and module lifetime | [module loading](module_loading/), [game ABI](abi/), [launcher](launcher_bootstrap/README.md) |
| Save/demo/navigation/archive data | [save callbacks](callbacks/), [canonical archive](save_archive/), [demo](demo/), [navigation](navigation/), [HPAK](hpak_records/README.md) |
| Audio and voice | [sound](sound/), [A3D module/device ABI](a3d_module/README.md), [selected output/coexistence](a3d_live/README.md), [capture](voice_capture/README.md), [mixer](voice_mixer/README.md), [caller wiring](voice_capture_wiring/README.md) |
| Steam and browser | [Steam](steam/), [service lifetime](steam_service/), [server](steam_server/README.md), [CEF](cef_host/README.md) |
| Text and UI | [text encoding](text_encoding/README.md), [localized resources](localization_text/README.md), [UI lengths](ui_lengths/README.md), [death notices](death_notice/README.md), [VGUI1 Debug](vgui1_debug/README.md) |
| Built Apple Silicon client | [actual-library UI, font and module-load checks](mac_client/README.md) |
| Software and dedicated | [software video](software_video/README.md), [lifetime](software_lifetime/README.md), [dedicated host](dedicated_host/README.md), [admin](dedicated_admin/README.md), [standalone VGUI](vgui2_standalone/README.md) |
| Browser | [model/provider](server_browser/README.md), [actual VGUI/lifetime/original-view cases](server_browser_ui/README.md) |
| Input and bounded repairs | [gameplay shortcuts](gameplay_shortcuts/README.md), [system capture](system_capture/README.md), [timer](hud_timer/README.md), [notify](console_notify/README.md), [multisource](multisource/README.md), [network](network_platform/README.md) |
| Native A3D contract | [buffers](a3d_buffers/README.md), [events/notifications](a3d_notifications/README.md), [resources](a3d_resources/README.md), [device](a3d_device/README.md), [spatial](a3d_spatial/README.md) |
| Renderer | [state/texture/ordered frames](render_state/README.md), [offline GPU shaders and adapters](render_gpu/README.md) |
| Resource admission | [precache and decal index bounds](resource_precache/README.md) |

To qualify a changed implementation, compare it with a pinned reference under
the same inputs and per-file policy. Preserve exact byte/count/state oracles and
failure cases; do not replace an expected result merely because native output
changed. Provider availability, component results, owning-file compilation and
full runtime acceptance are different evidence.

## Projects without a README

Use the common configure/build/CTest pattern above with the project directory.

| Project | Inputs and checks |
| --- | --- |
| `cpu` | Actual CPU discovery and fasttimer bodies; topology, counter units and conversion against an independent clock |
| `numerics` | `CSE_NUMERIC_SOURCE_DIR` selects current or pinned engine source; compares legacy conversion, angle bytes and cvar strings under their owning FP policies |
| `surface_lighting` | `CSE_SURFACE_IMPLEMENTATION` selects the implementation; `CSE_SURFACE_REFERENCE_MMX=ON` builds the Win32 reference. Preserve the pinned `expected_pixels.json` corpus/hash before changing expected pixels |
| `abi`, `callbacks` | ABI/table/callback and selected game-library fixtures; loading real game modules is separate from compile/layout checks |
| `demo`, `navigation` | Explicit legacy records, bounds and round trips; no full game replay or navigation gameplay acceptance |
| `module_loading` | `CSE_MODULE_SOURCE_ROOT` selects current/pinned SDK loader sources; actual owning/borrowed loads, factories and repeated leaf reloads |

For macOS ASan module-loading checks, set `CSE_MODULE_SANITIZE_LEAF=OFF` while
instrumenting the driver and production loaders. The external leaf alone remains
uninstrumented: a separate direct `dlopen` reproduction triggers Apple's ASan
ODR registration failure with an instrumented reloadable leaf. No global sanitizer
check is disabled. This is a fixture limit, not successful sanitizer leaf reload.

### Sound provider cases

Configure `tests/native/sound` with `CSE_SOUND_SDL_PROVIDER` set to the explicit
pinned architecture-matched SDL library. Windows also requires
`CSE_SOUND_SDL_IMPORT_ARCHIVE` for its matching import archive. Without a provider,
only transfer/ring cases build; the three provider cases are omitted.
Provider fixtures use SDL's dummy device and check format, PCM, ownership and
shutdown. They do not qualify physical effects output, rate conversion or voice.

### Steam provider and service cases

Ordinary CTest in `steam` / `steam_service` exercises incomplete-provider rejection;
it does not initialize a real SDK session. For explicit provider execution, verify
its artifact against [the lock](../../tools/qualification/fixtures/steam/lock.json)
and run `steam_provider_probe /absolute/provider --bind` for export binding, or
`steam_provider_probe /absolute/provider --client` for client initialization.
Set `SteamAppId=70` and `SteamGameId=70` in that process environment and use a
logged-in Steam client that owns Half-Life. These checks do not qualify server
authentication, encoded voice or complete gameplay.

For the real service fixtures, configure with
`CSE_STEAM_FIXTURE_PROVIDER=/absolute/pinned/provider`, then explicitly run
`steam_service_probe`. With AppID 10 (CS) or 70 (HL), invoke
`steam_service_module_probe` with absolute paths to its built owned module and
peer module to test repeated peer unload/reload and global cleanup.

`python3 tests/native/steam_service/check_staging.py --provider /absolute/provider
--target macos-arm64` checks restoration and hash-based staging rejection without
initializing Steam. Use `windows-x64` in the matching MSVC environment on Windows.

## Source provenance and binary inventories

Use the [qualification inventory/export commands](../../tools/qualification/README.md#source-and-binary-inventory)
to capture and verify a committed source reference, audit explicitly supplied
runtime architecture/imports, and compare native game exports against a retained
Win32 dump. Static inventories do not prove actual loading or gameplay. Preserve
exact upstream/reference/license text and pinned header baselines; approved
minimal active patches need their own hashes/provenance. The server-browser Valve
formatter patch preserves declarations, layout and output; its unmodified
upstream baseline remains separate. Build-policy checks use generated actual
owning commands, not replayed stale commands or fixture defaults.

Sound-provider, module-loading ASan, real Steam binding/service and inventory
commands above remain explicit boundaries. CTest dummy providers do not imply
physical output, live network/provider execution or full target acceptance.
