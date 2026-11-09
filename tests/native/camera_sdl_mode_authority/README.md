# SDL camera mode authority source fixture

This is a controlled source/component gate. It builds no SDL provider and executes no SDL_Init/provider/video initialization, physical input, window, game, UI or network service. It does execute the actual pinned SDL mouse PreInit/state/mode/warp/motion bodies against typed fake surrounding services. No production sampler or cursor-ownership implementation is supplied; production readiness remains false.

`extract_sources.py` requires the pinned SDL3.4.20 source hashes and imports39 complete SDL bodies unchanged. The complete unmodified `SDL_Mouse` and `SDL_Window` declarations are included from the vendor private headers. A byte-identical vendor minimal configuration supplies header feature macros; no dummy driver or SDK provider target is compiled or linked. This fixture configuration is not the frozen provider's linked-PDB layout. C11 bodies and C++14 consumers compare complete type sizes and eight relevant real member offsets as a fixture consistency check, without inventing a shadow SDL object or claiming original-image addresses.

The actual current `ApplyCursorVisibility`, both engine UI owners' complete request/commit blocks, complete HL/CS camera methods and common camera helper are exercised. Camera tests reuse existing controlled caller scaffolding with documented SDK query/warp seams; they are not engine/client ABI or full IN/worker proofs. The two UI blocks use typed pump/drain/game seams, with calls counted. Internal SDL dynapi names follow the real vendor header mapping; the fixture calls the imported implementations and does not initialize a dynamic provider dispatcher.

The only Win32 API-shaped initialization seam, GetDoubleClickTime, uses the real SDK signature and a controlled value. SDL hint registration invokes the imported callbacks with default values. Hash creation is deliberately a NULL allocation-failure seam: actual SDL_PreInitMouse still returns true; this does not qualify SDK resource creation or mouse-name registration. Unexpected touch/hash/rectangle paths accumulate a process-lifetime failure count, checked by the test. No unconfigured operation silently qualifies behavior.

## Observations retained by the assertions

* Failed emulation disable can leave globalrelative=true, publicwindowrelative=false, active=false, prohibited=true, while the public false setter returns true. `--naive-authority-hypothesis` must fail with exact assertion exit17. A normal passing run characterizes this fact; it does not fix SDL.
* Actual fresh PreInit defaults and prohibition-before-hidden-warps are tested. Changed request success/failure/rollback/retry, unchanged requests, focus/other-window cases, failed ShowCursor/hint disable, invalidation/pointer-ID reuse and fresh mouse lifetime reset are covered.
* From an explicitly qualified fresh sole-window history with emulation prohibited,4096 seeded actual-SDK operations preserve the one-way implication publicfalse→globalfalse. This is bounded control evidence plus a source-invariant candidate, not arbitrary-lifetime authority. Separate failed-disable/reuse/other-window controls demonstrate why generation/sole-writer premises are required. Publictrue is never claimed to prove actual globalrelative=true.
* Both actual UI request blocks commit their raw cache after failed enable; an unchanged request then suppresses retry even when the backend recovers. Caller pump/drain counts remain explicit. Desired/raw-cache state is not actual mode authority.
* Both native64 HL and CS receive one40,24 motion followed by three Think calls. Absolute, explicit relative with warp-motion false/true, and private emulation each change ideal yaw/pitch once by10/6. Each has three query/warp requests and zero camera relative drains. Cached position returns to center. Native driver warps are three for absolute and zero for relative/emulation. The relative accumulator remains40,24: cached recenter is not accumulator consumption. No repeated camera movement was observed in these controlled cases.

The retained fixture's IN hook is a controlled ownership seam; real IN edge drains/worker/lifecycle are separately qualified. SDL object-registry/window-storage invalidation is controlled here; no real SDL_DestroyWindow lifecycle is claimed. On x86 the full actual SDL mode/caller bodies execute, while native64 camera execution is explicitly excluded. Existing Win32 camera bodies are unchanged by this task.

## Run

Supply an existing pinned source tree, reused read-only:

```sh
python3 tests/native/camera_sdl_mode_authority/run_component.py \
  --repo "$PWD" --sdl-source /absolute/path/SDL3-3.4.20 \
  --build build/camera-mode-release --config Release \
  --evidence /absolute/new/evidence-directory
```

Use Debug and `--sanitizers` for the controlled sanitizer gate. The runner stops on configure/build failure before executing tests, checks both games and exact hypothesis exit17, and records commands/images/source-body pins. Mac policy is explicit-O2 Release/-O0,-g Debug, precise/no contraction, macOS14 deployment. SDK bodies compile as C11; caller scaffolding is C++14 without exceptions and with unwind tables. Actual compile commands are recorded; this is not a production owning-TU build-policy claim.

`run_negative_variants.py` builds seven deliberate source/seam variants in separate component build directories. Every variant must compile successfully then fail its assertion with exit17. Compiler errors and crashes do not count as behavioral REDs. The pinned vendor source and production source are never edited.

The sealed qualification packet lives under the separately owned `camera-sdl-mode-authority-v1` artifact directory. It retains initial harness errors and the first Windows fixture failure separately from final passing source snapshots. No root manifests, provider source, production source or frozen runtime images are modified.
