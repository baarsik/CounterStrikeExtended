# Validation and remaining evidence

Authoring environment: Apple Silicon macOS (`uname -s`: Darwin), CMake 4.4.5,
Python 3 standard library, local feature branch `codex/project-scope-inventory`.
No `cl`, `clang-cl`, or `msbuild` executable was found on PATH. A fresh local
`prlctl list -a -o uuid,name,status` inventory found two running Windows VMs.
Read-only compiler-tool inventory attempts using `prlctl exec --current-user`
failed for both with exit 255: Parallels could not open a guest session and
reported Tools/boot/isolation availability as possible causes. No guest command
was executed successfully, and no guest settings, credentials, boot or process
state were changed. Thus no suitable accessible Windows builder was available;
no VM access or runtime authorization was borrowed from another chat.

## Performed checks

| Check / command | Actual result |
| --- | --- |
| Project disposition versus original XML manifests | All 57 projects covered once: 16 keep, 3 optional, 38 approved removals |
| Original-manifest ZIP byte comparison against `git show c10c110:<path>` | 166 entries, zero mismatches |
| Live project/solution inspection after removals | 19 retained/optional projects; zero missing project paths referenced by remaining solutions |
| `python3 tools/translate_msbuild.py` | Translated 19 projects and both Win32 configurations; no unexpanded MSBuild expressions in generated target modules |
| Active compile/resource/library selection existence after pruning | 2,584 existing per-configuration selections checked; zero missing inputs; plus 10 modeled A3D generated selections (5 per configuration) |
| Conservative retained/optional include/forced-include scan before pruning | No consumers inside the 21 approved exclusive-source roots; shared/protected trees retained |
| `cmake --list-presets=all` | All four configure and build presets parsed: runtime/hardware × Release/Debug; no configuration/compiler checks invoked |
| Static COFF archive inspection | Miles/SDL2/Steam import archives identify x86 runtime DLLs; supplied public game/UI archives contain x86 objects |
| C1/C4 source slice comparison against original HEAD | Valid key map, forward switch and reverse initializer unchanged |
| C2 target/source crosscheck | Hardware and swds explicitly select WSOCK32 and link wsock32.lib in both configurations; modern Winsock2 modules remain separate |
| C3 source diff inspection | Hardware added only to process-header/terminal-exit guards; formatter, callback/logging/SDL/longjmp sequence preserved |
| `git diff --check` / staged diff whitespace checks | Passed for committed changes |

These checks establish source/dependency selections and bounded edit properties.
They do not establish successful CMake generation, compilation, binary ABI,
provider availability, runtime equivalence or playability.

## Not performed

- **No tests or CTest were executed**, following the explicit handoff prohibition.
- No CMake configuration or runtime compilation was attempted on macOS; the
  initial runtime target is Windows x86, not host macOS.
- No Microsoft MSBuild evaluation/preprocessing or Windows compiler preprocessing
  was available. `tools/export-msbuild.ps1` is supplied for evaluation only.
- No Windows Debug or Release build, PE architecture/import/export/resource
  inspection, module loading, game launch, provider/worker probe, network/audio
  activity or runtime/playability acceptance occurred.
- No push, release publication, live-game install or provider redistribution.

## Required next validation on a suitable Windows builder

1. Run `tools/export-msbuild.ps1`; compare actual Microsoft-default/import/item
   metadata against the repository-derived crosswalk. Check per-file options and
   Link definitions, including the recorded swds macro/export discrepancy.
2. Configure and compile the documented complete-runtime and hardware-only
   Debug/Release presets without weakening assertions, ABI, warnings, FP or
   provider requirements. Configuration/build failures remain failures.
3. Inspect resulting PE machine type (x86), output locations, exports/ordinals/
   decorated names, import-library compatibility, imports, resources and CRT
   dependencies. Confirm C2's actual SDK header constants and linked socket API.
4. Establish compatible required providers/data and redistribution rights. Any
   later test/runtime/provider/game validation requires separate authorization;
   this work does not supply it.

The owner's handoff explicitly permits completing source/build work when a
suitable builder is unavailable, with unbuilt/unverified results reported. This
is that source deliverable, not a qualified functioning binary package.

## Independent source review and fix pass

A [fresh read-only whole-branch review](source-review.md) checked the source/CMake/pruning/license
changes through `44ef186`, without executing tests, configuration or runtime code.
It found two material defects: shared import/PDB output basenames, and five
omitted A3D generated translation units. The fix pass isolates all linker/compile
products per target, models executable export archives, and includes all five
A3D section units with their original per-file settings. The translator now
rejects unrecognized local custom targets/tasks instead of silently skipping them.

The reviewer also noted that `/TP` alone leaves CMake classifying hlds's `mem.c`
as C. The fix sets target-local `LANGUAGE CXX` as well, so the inherited C++
language-standard policy applies independently of generator choice. It was not
an observed failure of the documented Visual Studio presets. This is corrected
as part of the requested C/C++ input translation rather than deferred.

Post-fix source checks verify distinct archive/symbol directories, all generated
A3D selectors and metadata against the original XML target, and the CXX source
property. Windows generation/compilation remains pending; the fix pass is not
reported as executed regression-test evidence.

The review deliberately did not judge runtime/PE/ABI results, complete legal
clearance, future architecture implementations, a software-selection UI redesign,
or a global change to the upstream swds macro/export discrepancy. These remain
outside the source evidence and/or the authorized narrow changes, as described
above and in the scope/correction/portability/licensing documents. Deletion
review used approved boundaries and retained references rather than reading
every removed body. No runtime, licensing or portability acceptance follows from
those bounded review decisions.
