# Native component fixtures

Subdirectories are independent CMake projects. Where present, their README
describes production bodies, controlled dependencies, covered inputs and
remaining integration work. The options below cover the simpler projects that
have no README. These fixtures do not collectively certify a
playable target; see [migration status](../../docs/native-migration.md) and
[validation](../../docs/validation.md).

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
| Save/demo/navigation/archive data | [save](callbacks/), [demo](demo/), [navigation](navigation/), [HPAK](hpak_records/README.md) |
| Audio and voice | [sound](sound/), [capture](voice_capture/README.md), [mixer](voice_mixer/README.md), [caller wiring](voice_capture_wiring/README.md) |
| Steam and browser | [Steam](steam/), [service lifetime](steam_service/), [server](steam_server/README.md), [CEF](cef_host/README.md) |
| Text and UI | [text encoding](text_encoding/README.md), [UI lengths](ui_lengths/README.md), [VGUI1 Debug](vgui1_debug/README.md) |
| Built Apple Silicon client | [actual-library UI, font and module-load checks](mac_client/README.md) |
| Renderer | [state/texture capture](render_state/README.md), [offline GPU shaders and adapters](render_gpu/README.md) |

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
