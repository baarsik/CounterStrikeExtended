# Engine system capture regression fixtures

The fixtures compile the production NativeMouseWindow/Cocoa adapter and extract
both complete SleepUntilInput bodies, ClearIOStates, client activation boundaries,
and both complete VGUI cursor owners. SDL queues, clocks, native query results,
and client/UI destinations are controlled seams; this is not a copied coordinator.

Configure with `CSE_SDL_INCLUDE` pointing at the pinned SDL3 public headers, then
build with at most two compiler processes and run CTest. Use Release, Debug, and
`CSE_SHORTCUT_ENGINE_SANITIZERS=ON` for ASan/UBSan. The inherited keyboard and
native_mouse_window suites cover the retained null-service/transaction behavior.

Coverage includes active/key plus foreign interaction; raw/grab/owner combinations;
input reset before deactivation; exact nine-class queue/timestamp cutoff;
lifecycle/quit under provider failure without native pumping; latest menu/custom
cursor intent; failure-stage retries; retirement/reentrancy; bounded 10 ms reads;
steady-state controls; fresh wheel pulses and post-setter motion through VGUI cache
acknowledgement. Timestamp zero is rejected for input after a transition cutoff.

No fixture launches an application or performs actual screenshot interaction.
Physical Cmd+Shift+4 drag/cancel/complete and subsequent gameplay/audio/fullscreen
acceptance remain controller-owned runtime gates.
