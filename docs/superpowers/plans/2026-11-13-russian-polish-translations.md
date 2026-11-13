# Russian and Polish Translations Implementation Plan

> **For agentic workers:** Execute the disjoint language authoring tasks in parallel using superpowers:dispatching-parallel-agents; integrate and validate in this session.

**Goal:** Bundle complete Russian and Polish interface text and prove representative translated UI text fits its controls.

**Architecture:** Extend the current manifest and catalogue tree, reusing the offline resource target. Author missing translations and reuse authenticated official text where available. Validate generated files through the actual loader, and use actual VGUI font/control measurements plus rendered samples for fit.

**Tech Stack:** UTF-8 source catalogues, Python 3.9+, CMake 3.25+, native VGUI2/SDL3 tests.

**Spec:** `docs/superpowers/specs/2026-11-13-russian-polish-translations-design.md`

## Global Constraints

- Languages `russian` and `polish` cover all current English token families and legacy interface resources.
- Preserve tokens, command syntax, substitutions, proper names and key bindings.
- Preserve all existing languages, aliases and offline/incremental build behavior.
- Validate actual widths/glyphs and inspect rendered samples; character counts alone do not establish fit.
- User authorized implementation; continue within the established architecture without another approval gate.

## Review Focus

- Cyrillic/Polish glyphs must exist and render through the selected font provider.
- Short controls must fit at minimum supported dimensions after padding.
- Long help/status text must wrap or remain readable without clipping.
- Placeholder variants and legacy byte limits must survive translation.
- Missing or unchanged English UI text must be explained rather than counted as translated.

### Task 1: Complete language catalogues

**Files:** `src/deploy/localization/**/*_{russian,polish}.txt`, localized legacy directories, `manifest.json`, catalogue tests.
**Interfaces:** Per-language workers own only their language files and supply a manifest-entry JSON fragment. Root owns manifest integration and shared tests.

- [ ] Extend scope tests for ten languages and complete English-token/argument/legacy parity; observe missing-language failures.
- [ ] Author Russian and Polish independently, reusing pinned official inputs where available; record provenance and any intentionally unchanged names/commands.
- [ ] Integrate fragments and language metadata; stage both packs and run the complete resource suite.
- [ ] Commit validated catalogue changes.

### Task 2: Validate and adapt fit

**Files:** Native localization/UI fixtures, fit evidence/report and translations needing adaptation.
**Interfaces:** Read actual staged catalogues through VGUI; measure real scheme fonts and actual control content bounds at minimum dimensions. Render representative Russian/Polish samples to images for inspection.

- [ ] Add failing actual-loader checks for Russian and Polish plus fit checks that detect a deliberately overlong label.
- [ ] Measure buttons/tabs/columns/status and representative stock dialogs; validate glyph coverage and wrapping with actual providers.
- [ ] Adapt translations or narrowly repair layout where needed; inspect rendered samples and rerun all affected checks.
- [ ] Run native owning builds and Windows portable staging/decoder checks; update maintenance and verification documentation.
- [ ] Obtain one fresh whole-change review, resolve material findings with regression checks, and commit the completed extension.
