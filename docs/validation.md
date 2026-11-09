# Validation status

No native target is fully qualified. The table records selected frozen cohorts;
later source edits require fresh build/runtime evidence. Windows results use
VM1, Windows 11 ARM64 running x86/x64 code under emulation. Mac fixtures run on
Apple Silicon with a macOS 14 deployment target; this is not a macOS 14 hardware
runtime qualification. [Remaining gates](native-migration.md#remaining-work-and-gates)
and [reproduction tools](../tools/qualification/README.md) are separate from these results.

## Native migration cohorts

| Evidence | Result | Limits |
| --- | --- | --- |
| V18 Win32 / Windows x64 Release and Debug | All 19 targets build; 21 Win32 and 30 x64 PE images per configuration; output/import/export/resource checks pass | Native Debug was interrupted at the disk reserve and resumed with identical inputs. Later HL length changes have owning-file/component checks, not a new complete cohort |
| V17 CS/HL x64 Release and Debug | Four selected gameplay/recording/full-replay cases; 651 frames and 15 pinned loaded images per case | Selected flows only; presentation, lifecycle, voice and numerical acceptance remain incomplete |
| V17 external Win32 server, x64 Release and Debug | Steam authorization, AMXX menus/HUD/messages/cvar replies, downloads, consistency and alive movement; 124 nonempty frames and 14 pinned loaded images per case | Unchanged ReHLDS 3.15.0.896 / Metamod-R 1.3.0.149 / AMXX 1.9.0.5303. No voice/audibility or live VAC-secure-bit claim |
| V11 data and server lifecycle | Eight selected Win32↔x64 demo replays, four HL save legs and selected reconnect/changelevel cases | Not complete entity, transition, C4 or navigation acceptance; not Mac game round trips |
| V18 production CS camera, x64 Release and Debug | Each has 1,041 alive frames, 199 movement-command frames and 410 exact rendered angle samples. Debug adds 18 read-only state samples. Separate multiplayer case retains third-person rejection | Threaded single-player camera only; no internal query/warp-order, atomic snapshot, other-mode or Mac camera claim |
| SDL_GPU renderer components | Three selected Windows D3D12 profiles and first native Metal profile match 12,288 GL RGB bytes each; R/D and Mac instrumented CPU variants checked | Only readback row order normalized. Production backend is unselected; other branches, precision formats and scene coverage remain open |
| Latest Mac compile discovery | Seven HL length diagnostics removed; 44 remaining file-location diagnostics, 26 unique after line/column removal | Full build still fails in Windows UI/platform/renderer/A3D owners. This count is not a percentage of port completion |
| Build-tool/component checks | 106 build-tool tests and 19-project translator check pass; selected component matrices include R/D, high addresses and sanitizers | Individual fixtures define their covered inputs; they do not establish complete game or provider acceptance |

The frozen producing-source/package manifests, imported/loaded-image hashes,
failed attempts and reports are retained outside the repository under:

```text
~/.codex/artifacts/cse-native64-20261108/
```

Useful entries: `runtime-platform-v18-*`, `native-camera-production-v18-final/`,
`hl-ui-lengths-v1/`, `render-metal-v1/`, `xcode-metal-component-v1/`, and
`macos-runtime-build-native-audit-v19-*`. Their reports and SHA-256 indexes bind
claims to exact inputs. A private machine path is not a substitute for rerunning
the checked-in [fixtures](../tests/native/README.md).

## Earlier Win32 reference

| Migration | Pinned implementation | Evidence |
| --- | --- | --- |
| SDL2 → SDL3 3.4.20 | `c4f729d`, compared with SDL2 `23a2135` | Four build presets, selected CS/HL R/D recording/full replay, input/focus, startup video, effects and dedicated-server checks |
| Miles music → mpg123 1.33.7 / SDL3 | `af5210a` | Decoder/player/worker checks, captured loop/pause/resume/volume/fade/replacement, selected CS/HL R/D replay and dedicated-server checks |

The SDL3 benchmark compared only those historical packages with matched providers;
its emulated-VM timings are not current-build or native-hardware performance claims.
One mpg123-era CS launch failed with `ucrtbase.dll` exception `0xc0000417`; unchanged
retries passed, but the cause remains unresolved. Exclusive fullscreen visuals,
physical devices/hotplug and complete non-ASCII UI acceptance were not established
by those runs.

Historical receipts remain under `~/.codex/artifacts/cse-sdl3-assessment-20261106/migration/`
and `~/.codex/artifacts/cse-mpg123-20261107/`; Git retains their original reports.
