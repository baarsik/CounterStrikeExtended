# Bound console notify rendering to the current message

The native CS110 1440p fullscreen keyboard test displayed junk after developer
notify messages. `Con_DrawNotify` in `GoldSrc-SDK/engine/console.cpp` reads
`g_szNotifyAreaString[charpos]` for every console column without checking the
256-byte array or its terminator. At2560px `con_linewidth` is318. A source-extracted
ASan/UBSan diagnostic reproduces a global-buffer-overflow at index256; the same
actual body completes at1920px/238columns. Exact source, commands and logs are in
`build/macos-runtime-bringup-20261109/notify-text-diagnostic-v1/receipt.json`.
The client message formatting and asset contents are separate paths; this repair
does not change them or attribute their newline behavior to this defect.

Stop notify rendering at the earliest of the current console-column limit, the
array capacity and the first NUL. A shorter replacement or cleared notification
must not show stale suffixes. A completely filled unterminated array must remain
bounded. Preserve every preceding character, font/color, position/width result,
notification timing/line bookkeeping and independent chat/debug drawing.
No public ABI/layout/save/protocol, client/asset, developer/FPS setting, input,
audio or renderer-selection change is required.

Approaches considered: enlarging the array still leaves an unbounded read and
would only move the resolution threshold. Disabling developer output hides a
real memory bug and changes requested diagnostics. The controller chooses a
small bounded, terminated iteration in the actual drawing owner, with a capacity
derived from its container. The user has delegated migration repairs and opted
out of questions, so no new approval pause applies. This is a small change; no
architecture redesign or separate builder check is needed.

Use complete actual-source function tests, Release/Debug and ASan/UBSan, preserving
original-source RED. Compile/link the owning native HW/SW engine tuples under
retained recipes with the latest accepted predecessor, and qualify owning
Windows Win32/x64 Release/Debug code without policy weakening. Controller alone
stages a reviewed immutable candidate and observes real CS1440p notification
messages without junk. That visual gate does not qualify the whole migration.
One source implementer, global at most two compilers, root/live/VM1 controller-only,
original Steam assets read-only and no VM2 or global permission/route changes.
The microphone startup issue remains explicitly deferred.
