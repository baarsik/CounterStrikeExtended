# CounterStrikeExtended Runtime Baseline Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. Tests are explicitly prohibited by the owner; use source/dependency inspection and suitable Windows configuration/compilation only.

**Goal:** Retain the approved CS/Half-Life runtime and provide an honest Windows x86 CMake baseline, scoped licensing, and C1–C4.

**Architecture:** Translate repository MSBuild inputs into checked-in target-local CMake manifests; use common target helpers for output layout and configuration without changing target-specific ABI policy. Preserve the upstream manifest snapshot as audit evidence. Keep runtime modules as real production-source targets and third-party libraries as explicit imported dependencies.

**Tech Stack:** CMake, MSVC v145/Windows SDK, Windows x86 C/C++/RC, Python standard library for source-manifest translation only.

**Spec:** Owner-approved docs/project-scope.md and the external counterstrikeextended-agent-prompt.md read in this chat. Implementation and removals approved on 2026-11-06.

## Global Constraints

- Retain Half-Life and Counter-Strike, hlds/swds, Miles, Core and DemoPlayer.
- Preserve optional broadcasting/A3D source and provide feature options (default OFF).
- Windows x86 is the initial target. Reject unsupported host/architecture combinations honestly.
- Do not execute tests, CTest, games, original binaries, runtime probes, Steam/network/audio workers or native providers.
- Preserve original MSBuild manifests until the CMake translation has been audited against their evaluated source/flag/dependency selections.
- Preserve ABI, exports, packing/calling conventions, per-file flags, Release assertions, CRT, FP/EH, include order and providers.
- Do not push, publish, borrow VM authorizations, modify the research repository, or claim runtime acceptance.

## Review Focus

- Debug versus Release must select the correct CRT and prebuilt libraries without inheriting CMake's NDEBUG defaults.
- Same-basename compile inputs and source properties shared by multiple targets must not leak per-file settings between targets.
- Imported CS source lists and particleman's forced includes must survive pruning.
- Hardware-only configuration must include VGUI1's generated compatibility import library and preserve hardware's no-ICF policy.
- A missing/incorrect provider, unsupported architecture, or missing Windows toolchain must fail clearly rather than substitute a stub.

### Task 1: Approved target scope

**Files:** docs/project-scope.md; GoldSrc-SDK/projects/vs2026/*.sln; archived upstream manifests; approved project files.
**Interfaces:** Produce 19 retained/optional live project manifests, immutable original-manifest archive and complete removal record.

- [ ] Record approval and preserve original project/props/items/solution/export/resource manifests before edits.
- [ ] Remove the approved 38 targets from active solutions and their live project manifests; preserve snapshot audit inputs.
- [ ] Check exact project coverage, retained dependency paths and solution references; commit the scope change.

### Task 2: Production CMake and crosswalk

**Files:** CMakeLists.txt, CMakePresets.json, cmake/, hardware/CMakeLists.txt, docs/building.md, docs/msbuild-crosswalk.md.
**Interfaces:** Produce 16 default production targets and 3 optional targets with real sources; output under build/runtime/<config>, symbols/libraries elsewhere.

- [ ] Evaluate repository imports/conditions and translate retained source, include, definitions, flags, libraries, resources and exports into per-target manifests.
- [ ] Add configuration-aware target helpers and imported x86 libraries, with no global ABI/FP changes.
- [ ] Add complete-runtime and hardware-only Debug/Release presets and commands; record unavailable Windows evaluation/build evidence explicitly.
- [ ] Inspect all translated selections and CMake syntax/source paths; compile only if a suitable authorized builder is available.

### Task 3: C1–C4 narrow source corrections

**Files:** selected VGUI2 key translation, engine/net_ws.cpp, engine/sys_dll.cpp, associated target link/backend definitions.
**Interfaces:** Preserve APIs and valid mappings; use backend-matched multicast constants and hardware fatal termination.

- [ ] C1: retain ASSERT_AT(false, 503), return -1 before invalid reverse-table access.
- [ ] C4: delete only the unused SDL decoder table/writes, retaining switch mappings and initializer signature.
- [ ] C2: pair explicit WSOCK32 engine target definitions/linkage with named loop/add/drop 4/5/6; preserve Winsock2 modules and record preprocessing limits.
- [ ] C3: include process.h and use _exit(-1) for Windows hardware terminal branches; preserve dedicated/non-Windows policy and callback/longjmp behavior.
- [ ] Inspect exact source diffs and valid-mapping preservation; record that tests/runtime were not run; commit separately.

### Task 4: Source pruning, licensing, portability and delivery

**Files:** approved 21 candidate roots only; LICENSE, LICENSES/, docs/licensing.md, docs/architecture-portability.md, README.md, docs/validation.md.
**Interfaces:** Consume approved scope and retained-source crosswalk; produce source-pruning ledger and accurate build/license/verification reporting.

- [ ] Remove only exclusive implementation/assets in approved roots after checking retained references; preserve provenance/readmes/notices and original-manifest archive.
- [ ] Audit retained local notices and authoritative upstream license; apply MIT only to authorized original contribution files and retain all inherited terms.
- [ ] Record x86 constraints and future x64/ARM64 source/provider/ABI blockers without claiming support.
- [ ] Request one fresh whole-branch code review, resolve material findings, run appropriate allowed static checks, and record actual builder/results/remaining validation.
- [ ] Commit reviewable changes and leave the feature branch local without pushing.
