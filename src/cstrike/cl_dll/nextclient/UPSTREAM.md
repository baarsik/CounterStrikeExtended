# NextClient radar provenance

Source: [CS-NextClient/NextClient](https://github.com/CS-NextClient/NextClient)

Pinned revision: `1d72f0a5f576ffd0e99207ac4edfec7468c6c218` (retrieved 2026-11-16).

The advanced HUD owner and `hud/radar/` modules originate from
`nextclient/client_mini/src/`. Radar cvar definitions originate from
`nextclient/client_mini/include/next_client_mini/radar_cvars.h`. The `ncl_math`
headers, `ncl_gl` helpers, and cvar definition helper originate from
`nextclient/packages/`. The bundled GLAD loader and Khronos platform header
originate from upstream `dep/glad/`; their generated notices are retained.
The shader sources originate from `nextclient/client_mini/assets/shaders/`.
The map corrections under `src/deploy/radar/` originate from upstream
`assets/cstrike/overviews/`.

Native adaptations replace binary injection with direct HUD, server-message,
smoke-event, primary-view and scoreboard integration; use SDL3 for graphics;
use a cancellation-aware C++ worker for layer analysis; and draw scalable
labels through this client's VGUI font service. The small optional profiler
adapter intentionally has no external engine/profiler dependency. The
GameUI dialog uses native controls and shared defaults, preserving the
upstream setting set and adding the HUD Radar entry under Options → Interface.

The owning engine GL model headers are used directly, preserving native
structure layout. `CSERadarEngine001` is an additive, versioned, process-local
renderer service for lightmap texture names, message sequence, graphics
generation and VGUI glyph lookup. Glyphs retain the radar shader clipping. Existing exported interfaces and network/save/demo formats do
not change.

Shader inputs are the files under `shaders/`. CMake embeds them into
`radar-generated/RadarShaders.h` in the selected build directory; edit the
GLSL inputs rather than generated output.

Upstream supplied no repository-wide license file at this revision; this
record does not add or change its licensing terms.
