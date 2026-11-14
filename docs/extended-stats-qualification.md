# Extended stats qualification

The implementation adds `HealthEx`/`BatteryEx`, the Win32/Linux32 `cse_extstats`
AMXX module, five-digit client rendering, independent CSE identity detection,
automatic/manual observation and spectator/broadcast snapshots. Installation and
API details are in [extended-stats.md](extended-stats.md).

## Verified on 2026-11-14

| Check | Result |
| --- | --- |
| Win32 module | MSVC 19.44 x86 Release builds; PE32 and AMXX/Metamod exports verified |
| Linux32 module | GCC 11 multilib and GCC 10.2.1 manylinux2014 i686 Release builds; ELF32 and AMXX/Metamod exports verified |
| CS client | macOS ARM64 AppleClang 21 Release target links |
| Focused production-code fixtures | 38/38 CTests with ASan/UBSan, including six probe-analyzer assertions |
| Existing fixtures | HUD timer 11/11, UI lengths 15/15, live-localization snapshot plus nine owner suites |
| Resource/build tooling | Localization catalogue 3/3; runtime source regeneration passes |
| Pawn source | Example and qualification plugin compile with AMXX 1.9.0.5303 |
| Win32 AMXX 1.9.0.5303 | Corrected module and natives run; all 14 boundary values from 0 through 100000 match actual entity readback and MSG_SPEC callbacks, including death/respawn |
| Win32 AMXX 1.10.0.5488 | Corrected module runs; all 14 boundary values from 0 through 100000 match actual entity readback and MSG_SPEC callbacks, including death/respawn |
| Linux32 AMXX 1.9.0.5303 and 1.10.0.5488 | Minimal plugin resolves all four natives and sees Ex IDs 66/67 before Steam runtime startup fails under i386 emulation |

The delivered Linux archive uses the older-glibc build: pinned manylinux2014 i686
image `sha256:7ff084c274da66ed891df826b97cd5bb1b1d36db2cc86e4f052ce47cb4b69ead`,
GCC 10.2.1, glibc 2.17 headers, CMake 4.4.4 and Ninja 1.13.2. Its highest required
GLIBC symbol version is 2.4, and libstdc++/libgcc are linked statically. The newer
Ubuntu/GCC 11 comparison build requires glibc 2.34 and is not the delivered Linux
archive. For a reproducible compatibility build, use the named preset inside the
pinned image, with a fresh build directory and its 32-bit Ninja on PATH.

The live fixture uses ReHLDS 3.15.0.896, Metamod-R 1.3.0.149 and ReGameDLL
5.30.0.814. Exact downloaded input hashes, binary hashes, console captures and
machine-readable coverage are retained under the implementation worktree's
`build/qualification/extended-stats/` directory. Third-party game binaries/assets
are test inputs and are excluded from the module packages.

A fresh branch review found six issues. Regression tests first failed, then passed
for legacy message lookup after game activation; queued kill/setter operations
inside message callbacks and connection reuse; stable publications retaining new
callback requests; legacy-only corrections in manual mode; canonical HUD reset and
legacy ordering on demo reversal; and spectator language replay using owned inputs.
The buffered legacy hook fixture also verifies correction targets the spectated
subject, follows the completed original in the same destination, and respects
AMXX BLOCK_ONCE. This keeps unreliable originals ordered before their corrections. Final source checks also found narrow delta health still reaching predicted weapon
life transitions and dead-view roll. Both consumers now prefer valid local Ex state,
with regressions observed failing then passing. A connection-ordering regression
also verifies queued setters survive AMXX entry forwards before module initialization,
while disconnect still discards the preceding connection's operations. The final focused suite passes.

## Remaining acceptance

Compilation and server callbacks do not establish downstream visual acceptance.
The cloned macOS UI fixture could not finish startup reliably; debugger traces
reach browser-process creation and resource case resolution during viewport
startup. Duplicate test instances also affected the initial launch comparison. Linux live testing reaches
a Steam runtime segmentation fault under QEMU i386 after native resolution. The boundary plugin now performs normal spectator/team selection; its final Win32
1.9 run passes zero-health kill and respawn as well as all positive boundaries. The AMXX 1.10 fixture passes the same sequence after staging its complete game data.

Still required before rollout: a Windows client build, local HUD inspection at
640x480 and larger resolutions, joined-player kill/damage/armor behavior, the
stock/updated client and old/new server matrix, ordinary spectator switching,
reconnect/map changes, and an unmodified HLTV relay's stable late joins and recorded
demo seeking. Those results remain **unverified**. The probe reports these dimensions
separately and cannot turn a successful server callback log into client acceptance.

## Implementation decisions

- Used a managed isolated checkout to protect the original checkout. Integration
  remains a separate action; the implementation stays in the attached worktree.
- Continued focused verification despite a pre-existing HUD timer extraction
  dependency failure. The fixture now includes its real dependency and passes;
  no HUD test coverage remains blocked by that baseline problem.
- Implemented the shared identity/publication state together because lifecycle and
  deferred output share the same backend. SDK message hooks retain separate ownership.
- Canonical legacy corrections pass through Metamod/AMXX hooks. A plugin can see
  both the original message and a correction; blocking remains in the hook path.
  Legacy-only correction queues do not update the Ex cache in manual mode.
- Registered Ex names through Metamod before plugin initialization and resolve
  game-owned legacy IDs after activation. Incompatible Ex payload sizes disable
  extension transmission instead of adopting an ambiguous format.
- Added explicit English fallback entries to the other seven project language
  catalogues because generation requires token parity. English, Polish and Russian
  contain the requested translations; other new labels remain English.
- Committed the coupled server/client implementation and fixtures together so its
  complete test target is runnable at that commit boundary.
- Kept float gameplay storage and its precision limit as designed. Large adjacent
  integer requests can produce identical actual float readback; transport and
  client state retain the full signed 32-bit domain.
- Reported unavailable downstream environments explicitly. The cost is outstanding
  rollout acceptance, rather than an unsupported claim of complete runtime compatibility.

No minor review findings were deferred. Changes remain local; no merge or publication
was performed.
