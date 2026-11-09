This is a source/platform component gate for the currently selected InputWin32.cpp.
inputosx.cpp is not selected or edited. The actual shared class, factory interfaces,
focus/context/event routing, key text table and Windows branch remain intact.
The new helper is a private finite VK-domain/uint16 mapping, not a generic System
key-state implementation or an exported interface.

Native reconciliation captures SDL_GetKeyboardState and SDL_GetModState once
only after the existing default-context and once-per-frame guards. It copies nine
down states plus three lock toggles; aggregate Shift/Control/Alt use the left/right
OR. It never pumps events or stores SDL's borrowed array. An owner thread must
serialize pumping/input operations; the two SDL reads do not create an atomic
hardware/message-queue snapshot. A real SDL owner must be initialized and pumping
before use. No constructor/static startup query is added. Missing/short snapshots
fail explicitly. Unknown private key codes fail without writing a result.

The packed word retains low mask 0x0001 for toggles and high mask 0x8000 for down, including
the existing loop's high byte 0x80 versus bool true (numeric 1) re-press behavior. The original
12-row order, query-before-ignore placement, recursion and frame/context guards
are tested rather than normalized. System.cpp is not changed.

Cursor methods delegate exactly through existing Surface callbacks when present.
Native false diagnoses an unsupported provider and aborts; no fabricated position,
no-op warp or host-global cursor API is substituted. A real native Surface must
implement/delegate these functions before full VGUI2/runtime acceptance. Surface,
provider, full UI, IME/focus and cold-load readiness remain false/unqualified.

Fixtures import exact methods, translation assignments, toggle table and native
boundary functions with source/body hashes. Method/state/cursor bindings are
controlled models, NOT the real CInputWin32 layout/vtable or full focus/context
runtime. They use actual KeyCode/HInputContext declarations. No real SDL keyboard,
OS GetKeyState, cursor movement, renderer or provider executes. Every unrelated
cursor-bounds callback aborts. Whole owning TU object compiles separately check
actual declaring headers, class and SDK context.

run_windows_matrix.py uses real SDK constants/call types and synthetic packed
words across MSVC x86/x64/ARM64 R/D. All six references must agree on 6,144 rows;
49,152 known packed states and 24,576 unknown/output cases also execute per profile.
run_mac_matrix.py compares imported methods to that golden R/D/ASan+UBSan,
executes the protocol oracle and four expected failure children, then compiles
both actual owning contexts with ART-only outputs. Existing NULL-to-VPANEL
warnings remain visible under unchanged root policy. check_windows_tokens.py
uses the separately frozen rectangle token reader (copied read-only into ART)
for six whole-TU selected-token equalities and a rejecting selected mutation.
run_negative_controls.py must reject seven isolated source/header mutations.

The owning manifests provide native SDL headers, linking and staging.
The component adds no System/Surface/provider/manifests edits and makes no full
runtime, physical keyboard timing, focus, IME or UI acceptance claim.
