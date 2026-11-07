# Licensing and provenance policy

CounterStrikeExtended is a mixed-provenance development fork, **not an MIT
relicensing of GoldSrc or the inherited SDK**. The owner authorized MIT for
original contributions. The root `LICENSE` scopes that grant to original build
orchestration and prose for which the authors hold the necessary rights; no
copyright ownership is invented for inherited material. Original source edits
inside inherited files do not erase the applicable inherited terms.

## Component audit

Paths in this table are relative to `GoldSrc-SDK/`.

| Retained component / origin | Evidence inspected | Policy and unresolved rights |
| --- | --- | --- |
| Counter-Strike and shared game code; Valve/Id notices | `cstrike/cl_dll/cdll_int.cpp`, `cstrike/dlls/cbase.h` explicitly restrict use/distribution/modification to non-commercial enhancements of Valve products, with other uses requiring written permission | Retain notices in place. No unrestricted MIT or commercial redistribution grant is established. Bots, hostages, private/public/shared CS code retain their original provenance. |
| Half-Life client/server and shared movement/game code | `cl_dll/cdll_int.cpp` has the same Valve/Id restriction; retained `dlls/`, `game_shared/`, `pm_shared/` and shared interfaces remain inherited | Published SDK terms may apply to matching published SDK material; do not assume every copied/reconstructed file is covered. Half-Life remains retained, not relicensed. |
| Engine, launcher, filesystem and dedicated runtime | Source inherited through BlackShadow/GoldSrc-SDK; no documentary whole-engine license grant was found in the inspected clone | Source availability and the fork's build changes do not establish distribution authority. Original/reconstructed engine provenance and rights remain unresolved. Do not represent the engine as MIT. |
| Tier0 and VGUI2 | `tier0/dbg.cpp` and `vgui2/src/Surface.cpp` state Valve ownership and permission/contract-based use/copying | Preserve these notices; a public game-SDK license is not proof of a grant for every runtime-library implementation. C1/C4 remain changes to inherited VGUI2 code. |
| VGUI1, GameUI, particleman and copied UI/control source | VGUI1's `utils/vgui/include/VGUI.h` and other inherited Valve notices; GameUI/private copies and particleman input boundaries documented in the crosswalk | Preserve all source/headers, notices and provenance used by retained targets. No separate blanket grant is inferred from their compilation or exports. |
| HLTV Core/DemoPlayer and optional broadcaster | Inherited Valve source/private copied headers; Core's `private/utils/bzip2/bzlib.h` includes Julian Seward's older bzip2 conditions | Preserve the private bzip2 notice as well as package bzip2 notices; they are different copies/versions. Disabled broadcasting does not eliminate Core/DemoPlayer or their notices. |
| Steamworks headers/import library | `external/steamworks/README.md` attributes headers to Valve Source SDK commit `be57e3d01ac7d7aa6a2cacbfef187c0b46616db0`; `steam_api_common.h` retains Valve copyright. Referenced local evidence files are absent in this clone. | Keep headers, README and notices intact. The README's origin statement is inherited evidence, not a verified Steamworks redistribution agreement. Obtain applicable Steamworks terms/authorization and verify provider/version compatibility before redistribution. |
| libmpg123 | `external/mpg123/` pins unmodified upstream 1.33.7 source, COPYING and AUTHORS; the shared decoder is built from that archive | LGPL 2.1; stage the license, attribution and complete corresponding source with the shared DLL, permit library replacement, and retain all upstream component notices. No Xash GPL integration/stub is included. Miles imports and bundled artifacts have been removed. |
| SDL3 | Official SDL3 3.4.20 package; `external/SDL3/LICENSE.txt`, unmodified selected upstream headers/import library/DLL, and `UPSTREAM.md` pin provenance | Preserve the zlib notice and unmodified upstream payload. Debug/Release consumers share the upstream x86 DLL; this is not a separate Debug SDL build. The retired SDL2 header notice is preserved as historical attribution in `LICENSES/retired-sdl-notice.txt` (repository-root relative). SDL licensing supplies no rights to Steam/game-data providers. |
| Codec/font/compression packages | Existing `external/installed/x86-windows-static-md/share/<package>/copyright` for bzip2, brotli, freetype, expat, libpng, libogg, libvorbis, libwebm, libvpx and zlib; additional fontconfig/dirent notices retained | Keep complete package notices. They contain different licenses/choices and attribution conditions; do not replace them with MIT. Review selected license alternatives and binary-distribution obligations for the actual package versions/configuration before shipping. |
| Prebuilt UI/control libraries | `lib/public/game_controls.lib` and `vgui_controls.lib` contain x86 objects; their presence does not establish complete source correspondence or a separate redistribution grant | Preserve without relabeling. Source/CRT/version provenance and permission remain unverified. |
| Optional Aureal A3D | `a3dapi/a3dapi.rc` identifies Aureal Semiconductor and 1996–1998 copyright; adapter loads legacy A3D provider | Keep source and notices; no permissive grant or provider redistribution right was established. Default OFF does not resolve source rights. |
| Remaining third-party SDK/tooling materials | Unmodified `external/` and installed-package notices, including 3ds Max, GLUT/glaux and tooling | Approved pruning did not authorize wholesale third-party deletion or relicensing. Preserve their terms even where the corresponding runtime build no longer uses them. |
| Archived upstream manifests / generated crosswalk | Original bytes in `docs/msbuild/upstream-manifests.zip`; generated source/flag/dependency selections | Archive retains inherited terms. Generated build-selection data does not confer a license to the selected implementation. |

The residual-game cleanup preserves ten inherited Blue Shift header files under
`GoldSrc-SDK/particleman/compat/` and the former DoD interface definition at
`GoldSrc-SDK/cstrike/public/game_controls/iclientvgui.h`, with unchanged contents
and applicable notices. Nine inherited documentation files are preserved under
`docs/upstream/`. Relocation does not change their origin or license or make them
original MIT contributions.

This is a component-level audit of inspected notices and dependency families,
not proof of complete file-by-file chain of title. Missing notices and unclear
origin remain unresolved rather than treated as public domain or MIT.

## Published Valve SDK reference

The [official Half-Life SDK license](https://github.com/ValveSoftware/halflife/blob/master/LICENSE)
allows specified free SDK/modification distribution subject to its conditions;
commercial use requires contacting Valve. It is restrictive compared with MIT.
The text was retrieved on 2026-11-06 and is preserved unmodified as
`LICENSES/Valve-HL1-SDK-reference.txt` (repository-root relative), SHA-256
`131212c5a670489679af77614fed8089c7ed71ce9ae3aa9333ff7718b643af84`.

That reference does **not** establish a license for all reconstructed engine,
VGUI/Tier0, Steamworks, A3D or other files in this fork. Its inclusion
preserves relevant published terms without asserting blanket applicability.

## Contributions, notices and redistribution

- Use `LICENSES/MIT.txt` only within the original-contribution scope in root
  `LICENSE`. Do not add MIT SPDX tags to inherited files indiscriminately.
- Keep copyright/attribution/license/provenance files and embedded notices in
  retained code and dependencies. Source pruning preserves the prose/notice
  candidates listed in `source-pruning.md`; original manifests remain archived.
- Before publishing a binary or source package, establish rights for the actual
  retained code, providers, assets and bundled dependencies and carry all their
  required notices. No release or binary redistribution was performed here.
- Blanket MIT coverage would require documentary authority from the relevant
  rightsholders for inherited Valve/Id and other content, plus appropriate
  third-party/provider rights. That authority has not been supplied; blanket
  coverage remains unresolved. This does not block the owner's authorized local
  build/source work.
