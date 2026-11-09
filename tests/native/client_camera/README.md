Native64 camera input uses the owned Runtime10 coherent position/center callback
and the existing engine warp. Win32 keeps its selected legacy implementation.
The native path tracks drag and distance intent separately, releases ownership
after the final recenter, and clears ownership on FirstPerson/ClearStates.
Query/history failure leaves camera offsets unchanged and issues no frame warp.

The fixture extracts actual camera state and complete methods from both clients.
It uses controlled engine/HUD/input services and the real scalar/key/cvar shapes;
it is not a replacement engine, an ABI proof or a physical-input test. Imported
HL unused locals/parameters and narrowing diagnostics retain their fixture-only
warning policy; new helper code is compiled outside that diagnostic scope. Actual
owning translation units are checked separately with their real per-file flags.

Checks cover repeated/nested commands, failure before ownership, forced release,
flag/hook ordering, final recenter, geometry changes, one query per active frame,
the distinct HL/CS distance rules, zero sensitivity, failure without partial
history changes, and reverse subtraction at INT_MIN. Wide subtraction followed
by the original integer division preserves ordinary values without overflow.
No window, device, SDL provider, clock, clipboard or network is used.

Configure/build this directory and run CTest in separate Release/Debug builds.
Use `CSE_CAMERA_SANITIZERS=ON` for ASan/UBSan. Native execution requires 64-bit
processes; Win32 is covered through actual owning compilation and selected-token
comparisons. Linking the complete client also requires its real
`IN_SetCameraMouseOwnership` implementation. The worker component and full game
input/focus/DPI/camera acceptance remain separate gates.
