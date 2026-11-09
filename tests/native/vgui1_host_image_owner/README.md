# Unselected Apple main-image owner

This C++14/NoEH component owns real loader references with fixed storage: one
exact-image anchor and one retained failed-balance reference. It supplies distinct
HostReleasesV2 semantics, not the frozen Runtime V1 record/contract. It does not
implement AppCold admission, a publisher, a factory, startup/shutdown binding, font
selection, SDL or UI. No production manifest/caller is changed.

`Open` accepts a caller-owned initial handle only on Ready. It verifies the
CreateInterface symbol/base/canonical path/loaded UUID and the direct loader
handle. Darwin dlsym follows dependencies; the proxy fixture demonstrates why
symbol identity alone was insufficient. A single RTLD_NOLOAD validation reference
is checked-closed. If that release fails, Open adopts NO caller reference but
retains its OWN validation reference in Closing/ReleaseFailed. The caller must
Inspect/Close before destroying the owner; process exit may retain the whole
context on permanent failure. No destructor calls the loader.

The normal NativeModule load-handle convention is required. Handles with other
lookup tags (for example RTLD_FIRST) are not decoded or silently aliased. A handle
value cannot prove reference ownership or an OS refcount: each supplied owning
reference must be a genuine separate load. Equal values are never deduplicated.
The process host must assign globally nonreused epochs; this object additionally
rejects non-increasing local reuse. Its path/UUID checks do not replace pre-load
ABI or cryptographic image admission, and staged files must remain immutable.

EnterRoot precedes an actual root call and LeaveRoot follows its return. An owned
anchor proves retention; otherwise a genuine exact-image reference must be held
by its real owner throughout the entry or transferred as the first anchor. A
borrowed mapping query or old function pointer is not such retention. The first
transfer is retained; later transfers are balanced by main-image code while the
anchor remains owned. Depth zero and separately confirmed root quiescence are
required for Close. Owner state/context must remain alive through every failure.
All methods except Contract/OnOwnerThread are owner-thread-only; invalid boolean
callbacks reject other threads before reading mutable state. Reentry is refused
while the local release boundary is running. Concurrent lifecycle/destruction is
not supported.

Fixtures use independent actual dylibs and the production NativeModule load and
symbol API. The new local release boundary checks dlclose return/error text because
generic UnloadModule discards the OS result. The ordinary target runs actual
dlclose. A separately labelled whole-TU variant changes ONLY the dlclose callee
to a typed wrapper that can fail BEFORE calling the real function. These controlled
failures preserve valid ownership; no arbitrary handle, double-close or forced
real OS loader failure is executed. Real valid-handle OS release-error semantics
beyond these controls remain unmeasured. No callback exception may escape; the
source itself uses no C++ allocation, exception handling, RTTI or floating math.
Global-new rejection covers component calls; loader/libc internal allocations are
not claimed absent.

Tests cover direct/dependent foreign handles, main-image callback identity,
zero/reused epochs, nested actual root callbacks, missing anchor/proof refusal,
20,000 real transfers under the initial anchor (including concurrent invalid
worker callbacks), 10,000 after dropping the original host reference, exact
bounded retained counts, query/extra/anchor failure retention and retry, reentry,
and actual final unmapping only after root return. False accepts nothing.
No unsafe callback-stack self-unmap is used as a negative control.

`run_macos.py --art NEW_DIRECTORY` freezes sources, builds Release/Debug/ASan+UBSan
and an owned Unicode/path-space Release profile, checks actual CPU/minOS/imports,
runs every case with a 45-second watchdog, and compiles the new source using
derived actual launcher recipes. A separate TSan owner case checks the worker
rejection while the owner mutates. New-source derived recipes are NOT actual
production membership/build claims. Earlier VGUI Font vtable ASan reload REDs and
independent virtual-class control stay immutable. This simple root has no C++
vtable and its own repeated instrumented loads are measured separately; passing
them does not qualify real VGUI/engine instrumented reload or font/UI lifetime.

The TSan race gate uses an explicitly separate 128-transfer burst after a full
10,000-transfer TSan case timed out at 45 seconds. The unchanged ordinary and
ASan/UBSan stress domains remain 10,000 per burst. The reduced race case is not a
claimed TSan stress pass; its count/flags and the original timeout are preserved.
Positive fixture cohorts use a process-local monotonic epoch issuer; the explicit
old-epoch refusal test intentionally supplies an already used value and is rejected.
