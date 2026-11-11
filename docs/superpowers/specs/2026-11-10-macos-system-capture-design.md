# macOS system screenshot mouse ownership

The user cannot select a Cmd+Shift+4 screenshot region during gameplay because
the game keeps the mouse at its center. Region selection must work, then normal
relative camera input must resume without a visible pointer, delayed motion or
a jump. Preserve the already accepted fast turns, scrollwheel bhops, menu
transitions, fullscreen behavior and normal audio.

## Evidence and current boundary

The user reproduced the issue in the CS110 shottrace session. Existing cursor
tracing recorded native active/key flags still true and relative mode still 2
while the frontmost mouse hit-test window was foreign. Overlay timestamps were
not separately recorded, so the hit-test signal needs qualification rather than
being treated as a finished detector. Evidence and the source trace are under
`build/macos-runtime-bringup-20261109/user-runtime-findings-20261110/`.

`NativeMouseWindow::RefreshCursorPolicy` currently reapplies presentation policy
and repairs an invisible cursor image. It does not relinquish SDL relative mode
or grab. The Cocoa image repair already yields to a foreign hit-test owner.
The ordinary focus sampler checks native active/key/geometry, which can remain
true while the system screenshot overlay owns mouse interaction.

The raw-off mouse-motion branch in both `CGame::SleepUntilInput` bodies can warp
the pointer when actual mode becomes absolute. Clients can consume SDL relative
deltas while their mouse activation flag is set. Therefore releasing SDL mode
alone is insufficient. Reacquisition must also preserve the existing pre-setter
motion cutoff and the versioned UI ownership transaction.

## Approaches and controller decision

Use a public, synchronous native interaction-ownership query together with an
engine-private capture coordinator. Keep logical UI intent separate from
effective gameplay capture. This handles system overlays without identifying
their process, installing an event tap, or changing system preferences.

A shortcut-only solution cannot reliably observe the end of an overlay and may
miss a key intercepted by macOS. A timeout would recapture the pointer while the
user is still selecting. Disabling relative input globally would regress the
accepted camera behavior. Those approaches are outside this design.

Apple's public `NSWindow` hit-test API returns the frontmost mouse-down recipient
at a screen point and excludes windows that ignore mouse events. It is already
used in the owned Cocoa provider. Reference:
[windowNumber(at:belowWindowWithWindowNumber:)](https://developer.apple.com/documentation/appkit/nswindow/windownumber(at:belowwindowwithwindownumber:)?language=objc).

## Provider contract

Propose an additive fixed-width C export in the existing owned SDL provider:
`uint32_t CseSdl_CheckCocoaInteractionOwnerV1(uint32_t version, void *nativeWindow)`.
It borrows the genuine current SDL Cocoa window property synchronously. Preserve
all existing enum values and exports. Add one distinct result for a valid owned
window whose interaction belongs elsewhere; do not conflate it with contract,
dispatch, thread, readiness or invalid-window failures.

The query checks the existing same-image/main-thread/video/window guards, native
active/key/minimized state, valid content geometry and frontmost hit-test owner.
It is independent of current absolute/relative mode so the same check can
authorize reacquisition. It pumps, consumes and warps nothing, retains no pointer
or callback, and uses no global input-monitoring or screen-recording permission.
Extend the private typed service seams only for the new reads; existing service
entrypoints must retain their behavior and readiness requirements.

## Engine capture coordination

The exact engine-private transaction signatures are a builder decision, subject
to a controller ruling before implementation. The required state and ordering are:

Native mouse interaction ownership does not by itself authorize or prohibit normal
focused UI keyboard delivery. The same NotInteractionOwner result can mean an
ordinary pointer outside an active/key window. Before any established gameplay
capture, and in ordinary visible/custom UI, preserve keyboard/text navigation
outside content. Distinguish that from an actual suspended effective gameplay
capture interval: while such an interval remains unresolved, all input fencing
and capture suppression still apply even if menu intent changes. Native errors,
retired identities and dirty shortcut retirement remain fail-closed for capture;
normal UI eligibility must not bypass them or restore stale gameplay capture.


1. Preserve the latest committed and pending UI intent: current owner, visibility,
   raw choice, gameplay/custom-cursor choice and requested grab. `dc_none`
   gameplay can request capture; `dc_user` remains absolute custom UI input.
2. A valid foreign interaction owner temporarily suspends effective gameplay
   capture. Retain UI intent, release relative mode and mouse grab, release the
   owned hidden-cursor presentation policy, and yield the system cursor image.
   Repeated polling must not repeat setters, drains or warps.
3. While suspended, neither UI owner may reacquire capture or replace the system
   selection cursor. New UI requests update intent and remain transactionally
   fenced. Both engine mouse-motion branches must skip legacy recentering.
4. Stop client camera consumption through the existing mouse activation boundary.
   Use the existing input-reset path to avoid stuck held input. Suppress foreign
   mouse/keyboard/wheel delivery to gameplay and VGUI, while continuing lifecycle,
   quit and window processing. Preserve command-buffer and supported `wait`
   semantics; do not flush or rewrite user commands.
5. When native interaction returns, reacquire only for the current live window,
   active gameplay and newest UI intent. A menu opened during selection stays
   absolute. Actual focus loss, minimization, teardown or window replacement
   must never reacquire using stale intent or a retired generation.
6. Clear old client/mouse ownership state and drain old accepted relative motion
   **before** the first reacquisition setter. No post-setter pump/drain may discard
   fresh motion. Resume client consumption after successful reacquisition.
   Queued events from the foreign interval must not become camera movement or
   selection clicks in the game; define and test the event cutoff explicitly.
7. Keep this separate from `AppActivate` audio/career behavior. Native app focus
   flags are not rewritten to simulate a system overlay. Normal music, voice,
   rendering and timing settings remain unchanged.

The outer engine loop can poll much faster than accepted render frames. Bound
expensive native hit testing to at most one query per 10 ms during steady
gameplay, without sleeping or creating a thread. Invalidate cached ownership
on window generation/lifecycle changes, and do not reuse it across owners.
Native failures retain a safe, retryable binding and must not authorize capture.
The builder should settle the minimal coordinator shape and reentrancy protocol
after inspecting `ClearIOStates`, both actual cursor callers and both event loops.

## Scope and constraints

Production scope is the existing engine-private mouse owner/adapter, the two
event-loop/cursor call sites and the owned Cocoa provider/header/service seam.
Do not change client mouse mathematics, raw acceleration/sensitivity/filtering,
bindings, FPS/VSync, global preferences, audio routing, permissions, CThread or
public engine/client interfaces. Preserve Windows behavior and pinned upstream
SDL bytes/dynapi; any new native export is owned CSE code in the existing provider.
The user's microphone startup-consent issue is explicitly deferred.

The current user-accepted CS110 application must remain untouched during source
implementation. It uses the earlier accepted engine tuple plus the corrected
timer client, fullscreen on the M5 Max GPU with normal RODECaster audio. A later
music-engine candidate once selected Apple's CPU OpenGL renderer; that separate
selection regression remains an acceptance gate. Production Metal integration
is also unfinished. Neither may be silently bypassed by this input repair.

## Validation and delivery

First qualify the public ownership signal with actual Cocoa behavior, then an
actual-source RED for the release/reacquisition boundary. Cover both HW and SW
event-loop owners, VGUI1/VGUI2, raw/grab combinations, custom cursor, held input,
selection cancel/complete, UI changes while suspended, wrong-thread/foreign
dispatch, setter failures, nested invalidation and window replacement. Include
positive controls proving ordinary gameplay does not repeat setters or discard
new motion, and that hit-test cadence stays bounded in the outer polling loop.

Use actual owning code and named OS/SDL seams, with Release/Debug and relevant
sanitizers. Build the changed provider and affected native engines/consumers under
their retained owning policies and regenerate the coherent launcher tuple.
Controller owns VM1, runtime staging, actual Cmd+Shift+4 drag/cancel/complete and
subsequent fast-turn/bhop/menu/audio tests. No source or component result alone
constitutes physical input acceptance or a completed migration.

Existing user authorization is to continue necessary migration fixes autonomously.
This design does not introduce an additional permission or approval pause. A
fresh builder check precedes the concrete implementation plan; only one source
implementer owns the worktree at a time.
