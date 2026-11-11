# Native source replacement policy preservation

**Goal:** Restore the maintained reference compiler policy when selecting the native BaseSystemModule replacement. Actual fresh full Windows x64 generation revealed differences that replaying old TU commands could not detect.

**Spec:** docs/native-migration.md; actual frozen Windows full-graph policy evidence supplied by controller in build/macos-runtime-bringup-20261109/full-windows-runtime-validation-20261110.md and artifact x64-release-policy-comparison-v18.json. Original source map/policies are authoritative; do not alter validation flags to hide differences.

## Global Constraints

- Preserve original Win32 source selection and all unrelated per-file numerical, exception, optimization, RTTI, language and runtime policies. Native width selection includes Windows x64 and Mac ARM64.
- Authoritative changes belong in retained .vcxproj metadata; regenerate tools/translate_msbuild.py outputs. Never independently edit generated CMake/crosswalk files.
- Actual required policy: native BaseSystemModule in hw_engine and swds retains /fp:precise; in GameUI and HLTV retains /EHsc. Both configurations must preserve the replaced source's policy. Other three native Base owners must not drift.
- Work only in the reused clean isolated native-system-modules worktree, branching from814e2ab. Root production/builds/apps, original Steam files and all VMs are controller-owned. No helper agents.

### Task 1: Preserve native replacement compiler policies

**Files:** GoldSrc-SDK/projects/vs2026/{hw_engine,GameUI,HLTV}.vcxproj, GoldSrc-SDK/swds/swds.vcxproj, existing tools/tests/test_native_runtime_policy.py (or a narrowly scoped policy test), regenerated cmake/targets/{hw_engine,swds,gameui,hltv}/Sources.cmake and generated crosswalk as needed.

- [ ] Capture current actual generated-policy RED against retained/reference policies. Add a meaningful regression that exercises production translation/projection into selected source flags in both configurations and checks native replacements against replaced reference policy; avoid a test that merely searches for the XML tags you add. Include unchanged Win32 selection/policy and other native owners.
- [ ] Restore the missing precise FP and synchronous exception policy in authoritative native Base source metadata, using existing translator conventions. Do not broadly change target defaults or enable exceptions for engine files that were deliberately disabled.
- [ ] Regenerate, run focused translator/native-policy tests and idempotence/diff checks. Inspect actual source flags emitted for Windows and native non-Windows selection, including all seven native Base owners. Record exact baseline versus corrected policy, generated hashes and commands/output.
- [ ] Commit only this task's change from BASE814e2ab and self-review. Controller performs immutable full generated Windows command-policy comparison and covering builds/links after review. Compile-only replay of old commands cannot qualify this fix.

No production C++ logic, ABI, cleanup or platform-feature changes belong to this task. The controller will integrate only the task diff and continue the larger migration without a plan-approval pause; user already authorized all necessary local/VM1 work.
