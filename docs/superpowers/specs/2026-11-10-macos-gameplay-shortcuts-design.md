# macOS gameplay Control-Space ownership

The user reports Control-Space switching keyboard layout instead of the expected
duck+jump combination. Gameplay must receive the ordinary Control and Space
events while the app avoids the layout-switch action. Preserve console/chat/menu
text entry, normal system shortcuts, the accepted mouse behavior and audio.
The user has delegated necessary migration actions and is no longer answering;
continue autonomously without additional questions. Human gameplay acceptance
that cannot be obtained remains unverified.

## Confirmed boundary

Diagnostic evidence is under `build/macos-runtime-bringup-20261109/`:

- `keyboard-shortcut-probe-v2/`: exact live SDL image f13d7f35…, video only,
  real native focus result1. Control-Space changes PolishPro to RussianWin with
  text input enabled, and back with text input disabled. SDL receives Space down
  and up in both cases. StopTextInput alone is not a fix.
- `keyboard-shortcut-probe-v3/`: a local NSEvent key-down filter prevents the
  AppKit layout-switch action while SDL still receives all Control/Space down/up
  events. Control-Shift-Space and text-input-off cases also pass. Disabling the
  filter restores layout switching and the original PolishPro source. The
  supervised probe exits0; no game, preference or permission change occurred.

These are native OS behavior tests driven by CUA, not human gameplay/performance
acceptance. Source, compiler flags, signed images, commands, timestamps and logs
are retained. The production client has not yet changed.

Pinned SDL3.4.20 `SDL3Application::sendEvent:` calls Cocoa_DispatchEvent before
`[super sendEvent:theEvent]`. SDL key delivery therefore precedes the app-local
monitor in NSApplication dispatch. This ordering is required evidence for the
solution: do not synthesize, duplicate, remap or suppress the already queued SDL
key events. Preserve the pinned upstream archive/source/dynapi bytes.

Apple documents [local event monitoring](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/MonitoringEvents/MonitoringEvents.html)
as an app-only dispatch filter that may return nil. Monitor handlers run on the
main thread and require explicit early removal. Global monitoring is not used.

## Approaches and controller decision

Text-input enablement does not explain or prevent the switch, as the A/B probe
shows. Broad keyboard grabbing would reserve unrelated system shortcuts and
conflict with the requested region screenshots. Use the demonstrated app-local
filter, enabled only for the current live gameplay keyboard context. No global
preference, event tap, hotkey registration, secure-input mode or permission grant.
This choice is the controller's implementation decision under existing clearance.

## Provider contract

Add a pure C header `common/sdl3_native_cocoa_keyboard.h` and an owned export in
the existing CSE SDL Cocoa provider:

`uint32_t CseSdl_SetCocoaGameplayShortcutsV1(uint32_t version, void *nativeWindow, uint32_t enabled)`.

Define keyboard contract version1; enabled is exactly0 or1. Reuse existing CSE
Cocoa result values without changing any of them; only Ready1 succeeds.
Preserve all existing API bodies/service readiness and mouse behavior.

The borrowed window must be the genuine current SDL Cocoa property. Validate
version, argument, same-image SDK/video/main-thread and owned-window identity
before native mutation, using existing qualified guards. Enable only for one
window at a time. Do not retain the borrowed window or an engine callback.
The monitor token is new provider-owned lifetime state, removed explicitly before
the owning window or SDK is retired. This setter is stateful; the existing pure
query/sampling contracts remain pure. Repeated identical requests must not
install/remove monitors repeatedly. Failure is visible and retryable, and must
not silently discard an installed owner.

For an enabled, current active/key owned window, filter only native key-down
events for kVK_Space with Control held and neither Command nor Option held.
Shift and either/both Control keys remain valid gameplay combinations. Other
keys, key-up, modifiers, unrelated windows and inactive/retired contexts pass
unchanged. Keep SDL's existing event delivery intact. No layout polling or key
logging belongs in production.

The exact private typed service/state signatures and scalar native identities
are left to the builder, with a concrete controller ruling before source edits.
The design must cover reentrant installation/removal/invalidation, failed native
admission, event-window identity reuse, window replacement and unload ordering.
Do not add a callback registry or a native event callback into unloadable engine
code. Tests must execute actual new provider bodies with named OS/SDL seams.

## Engine ownership

Expose the new policy through an optional engine-private platform service and
the existing versioned native-window owner, or an equally small adjacent private
owner. The builder settles the concrete signatures after inspecting the complete
current owner, both HW/SW loops and keyboard/text UI state. No public engine or
client ABI changes.

Enable only for a committed, current gameplay cursor intent and active gameplay
keyboard destination. Hidden custom UI cursors are not gameplay. Console, chat,
GameUI text fields and menus must keep ordinary layout switching. Cursor hiding
alone is insufficient to identify chat/console; inspect key_dest, con_forcedup
and existing GameUI visibility rather than inventing a new UI mode. Refresh
current keyboard intent before native pumping can dispatch a new key event;
honor subsequent UI intent changes and failed/pending cursor transactions safely.

Bind/release through the same current-window/generation/reentrancy discipline.
Remove the provider hook before window destruction or SDK unload, with actual
HW/SW lifecycle paths proving that failures propagate before retirement. Native
focus guards must prevent interference in other apps while inactive. The later
system-screenshot capture coordinator must be able to suspend this policy along
with effective gameplay input, while preserving the latest logical UI intent.

Do not change raw acceleration/sensitivity/filtering, bindings, the command
buffer, supported scrollwheel +jump/wait behavior, FPS/VSync, cursor mathematics,
AppActivate audio/career behavior, saves, assets or renderer selection. Do not
fix the explicitly deferred microphone startup-consent issue. Preserve current
accepted game files while building immutable candidates. Controller owns live
staging and VM1; never VM2. One source implementer at a time, at most two compiler
processes, retained owning build policy, no unrelated snapshot imports.

## Delivery and validation

Two reviewable source tasks: unused additive provider policy first; engine/UI
ownership integration second. Qualify actual provider Control-Space behavior
before enabling it in a game. Keep old APIs/guards regression evidence and pin
new native SDL images to unchanged upstream objects/source. Engine integration
needs complete actual-source cases for both event-loop/cursor owners, gameplay
versus chat/console/menu/custom cursor, transitions, failures, reentrancy and
teardown. Keep ordinary steady input free of repeated native setter work.

Build Release/Debug provider and affected native engine/consumer closures with
coherent genuine-main launcher admission. Controller qualifies precise Windows
owning TUs under retained policies. Native runtime gates include Control-Space
and Control-Shift-Space, ordinary Space/Control, real layout preservation, text
entry layout switching, focus/window/fullscreen changes, Cmd-Shift-4 and normal
audio/rendering. Verify a hardware renderer for hardware tests. The current game
continues to use OpenGL; production Metal remains unfinished. Automated native
OS checks do not become human fast-turn/bhop/performance acceptance.
