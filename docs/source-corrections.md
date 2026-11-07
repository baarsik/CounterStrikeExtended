# Narrow source corrections (C1–C4)

This record describes the four source corrections made before the SDL3
migration. Their initial evidence was source/build inspection. Later Windows
build and runtime results are recorded for a pinned snapshot in the
[SDL3 validation report](sdl3-migration-validation.md); those exercised paths
do not establish a dedicated runtime reproduction of each issue below.

## C1: invalid reverse-key return

In the `vgui2.vcxproj`-selected
`GoldSrc-SDK/vgui2/src/vgui_surfacelib/vgui_key_translation.cpp`,
`KeyCode_VGUIToVirtualKey` retains `ASSERT_AT(false, 503)` and returns `-1`
when the argument is outside `[0, 256)`, before indexing the reverse table.
This preserves the specified failure continuation if the assertion returns.
It does not establish a fix for ordinary movement or firing.

## C2: multicast constants paired with WSOCK32

Both retained consumers of `engine/net_ws.cpp` are `hw_engine` and `swds`.
Both link `wsock32.lib` in Debug and Release. Their live MSBuild manifests now
explicitly define `CSE_ENGINE_WSOCK32=1`; the translated CMake targets preserve
that selection. The Windows loop/join/leave calls use named constants `4`, `5`
and `6` for this backend. Non-Windows calls remain unchanged.

The source still includes Winsock2 headers, whose modern option values are
`11/12/13` in the handoff evidence. The initial correction did not record their
expansion in Windows SDK 10.0.26100.0; no multicast-specific preprocessing or
call-path proof is recorded here. The source requires exactly one explicit
Windows backend definition; a future
`CSE_ENGINE_WINSOCK2` consumer uses the header's option macros instead of legacy
values. No retained target has been switched to WS2_32. Core and other modules
retain their separate Winsock2 linkage.

No live multicast failure was established. This is a source/API-pair correction,
not a verified networking repair.

## C3: hardware fatal termination

`engine/sys_dll.cpp` includes `process.h` for Windows hardware as well as the
existing `SWDS_DLL` branch. The two terminal branches of `Sys_Error` now select
`_exit(-1)` when either hardware or the existing dedicated macro is selected.
The formatter's existing dedicated-only `_vsnprintf` condition is unchanged,
as are callback/logging order, SDL message box, recursive longjmp and
non-Windows policy. Normal quit and Steam-worker behavior are not changed or
qualified by this correction.

The upstream discrepancy that `swds.vcxproj` does not itself define `SWDS_DLL`
remains recorded in [project-scope.md](project-scope.md); C3 does not change that macro
and all the other conditionals it would affect.

## C4: remove unused SDL decoder writes

Only the selected VGUI2 source file's unused `s_pVirtualKeyToVGUI[256]` and its
reset/fill writes were removed. `InitKeyTranslationTable()` remains as a no-op
entry point for existing callers. At the correction commit, source-slice
comparison against upstream `c10c1100792ce1c23f7941ff350f4c4746151ee1` confirmed
that the forward switch, `keyMap`, reverse table and reverse initializer were
unchanged. The similarly named Win32 table in `Surface.cpp` and unselected source
copy were outside C4. Later SDL3 input changes are covered by the migration report.

This removes unread stores indexed by SDL key values such as `0x40000062`.
It is a source-level dead-state/undefined-behavior cleanup, not evidence of an
observed x86 crash or corruption; address wrapping and optimization can affect
emitted x86 behavior.
