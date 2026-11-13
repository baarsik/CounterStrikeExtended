# CMake Source of Truth Implementation Plan

> **For agentic workers:** Use the executing-plans workflow and verify each task. Independent fixture, test and documentation updates may run in parallel.

**Goal:** Maintain the build directly in CMake, removing Visual Studio project files and the MSBuild translator without changing existing build settings.

**Architecture:** Retain the current target and per-source CMake definitions as authoritative. Obtain selected sources and effective compiler settings from CMake configuration rather than a parallel manifest. Preserve generated ABI/save-callback tools as explicit source-generation utilities.

**Tech Stack:** CMake 3.25+, Ninja, Python 3.9+, MSVC/Windows SDK and Apple Clang.

## Constraints

- Preserve the existing src rename and all existing Debug/Release, architecture, source, export, resource and runtime output policies.
- Keep one CMake directory per target for source-property isolation.
- Keep bundled external source archives and historical provenance intact.
- Do not commit, push, launch games or mutate external providers.

## Tasks

- [x] Capture the current CMake definitions; implement and test a CMake-backed inventory/query for tools and fixtures.
- [x] Replace fixture and audit dependencies on Visual Studio manifests with CMake queries.
- [x] Replace translation tests with CMake configuration/recipe tests preserving meaningful policy coverage.
- [x] Remove active Visual Studio files and translator/export machinery; retain required export definitions and explicit source generators.
- [x] Update build documentation, contributor instructions and licensing path descriptions.
- [x] Compare preserved target settings with the captured baseline; run tooling and representative component suites and review the complete change.

## Review Focus

- Debug/Release diagnostics, CRT, assertion, exception and floating-point flags remain scoped and ordered.
- Platform-conditional sources, native aliases and main-image publication requirements remain selected correctly.
- Resource files, decorated x86 exports and native export projections remain available.
- Source generation follows CMake-selected game inventories and remains repeatable.
- Fixture introspection is identified as metadata inspection, independent of an actual cross-platform build.

## Verification result

- 85 tooling tests, 138 native component CTests and 9 main-image publication tests passed.
- Real full macOS Debug and Release runtime configurations succeeded (19 production targets each).
- All 45 original target/source/export policy bodies are unchanged except comments and moved x86 export paths; all five moved x86 export files retain exact bytes.
- Source generation reproduced 85 derived files exactly; active Visual Studio project files and translator references are absent.
- Full Windows compilation and game execution were not performed.
