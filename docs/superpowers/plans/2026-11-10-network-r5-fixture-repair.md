# Network R5 Fixture Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the retained optimized Windows network fixture build while keeping its excluded-path assertions meaningful.

**Architecture:** Replace two throwing controlled seams with counters and assert them after actual ordinary resend calls. Exercise the actual passive branches to prove that the guard catches excluded activity. Production source and build policy stay unchanged.

**Tech Stack:** C++17, CMake3.25+, Ninja, Clang ARM64, MSVC Win32/x64, CTest, retained Python source extractor.

**Spec:** `docs/superpowers/specs/2026-11-10-network-r5-fixture-repair-design.md`

## Global Constraints

- Modify only `tests/native/network_platform/send_tests.cpp`, `tests/native/network_platform/CMakeLists.txt` and focused fixture documentation/receipts needed for this change.
- Keep production files, source extraction, all existing cases and link libraries unchanged; preserve `/O2`, `/W4`, `/WX`, `/EHsc` and native sanitizer/warning policy.
- The original failed Windows Release receipts are retained; do not rerun or overwrite them to hide the original defect.
- One production/test source implementer at a time; queued until the current browser source owner yields. Global compiler cap2. Controller owns VM1 and live/staging actions; never VM2.
- No global routes/firewall, permissions, audio, original Steam assets, running fixtures or VM settings change.
- Use the repository C++ conventions for edited code, descriptive names and named domain constants; do not reformat unrelated code.

## Review Focus

- Ordinary unicast resend must fail if either excluded seam is called, even when the actual owner returns normally.
- Passive timeout with a challenge calls group leave and connect exactly once, without an unexpected datagram.
- Passive timeout without a challenge leaves the group without connecting and preserves the actual disconnect/reset behavior.
- Resetting a fixture removes prior seam counts so a failed/excluded branch cannot poison the next case or accidentally pass it.
- Optimized Win32/x64 and Apple/non-Apple controls execute the same actual extracted production owner without diagnostic suppression or compiler-known noreturn test chains.

## Task 1: Repair excluded-path fixture seams and qualify retained matrices

**Files:** Modify `tests/native/network_platform/send_tests.cpp`, `tests/native/network_platform/CMakeLists.txt`; update `tests/native/network_platform/README.md` only to explain the guard/new scenario. Do not edit production or extractor bodies.

**Interfaces:** The existing controlled `CL_SendConnectPacket()` and `NET_LeaveGroup(netsrc_t, netadr_t)` signatures stay unchanged. Add private fixture counters and a shared boolean predicate for no excluded calls. `Tick(double)` requires that predicate immediately after actual `CL_CheckForResend()`; deliberate passive-path regression invokes the real owner without going through the ordinary unicast Tick assertion and then checks the same predicate returns false.

- [ ] Step1: Verify exact current fixture/production inputs against `build/macos-runtime-bringup-20261109/network-r5-fixture-investigation-v1/source-pins.json` and immutable original Windows RED reports. Read the spec and actual passive branch before edits. Report drift instead of importing an old snapshot.
- [ ] Step2: Add the meaningful actual passive-path regression with challenge1 then0 in separately reset state, time31seconds and no real network. Establish that the prior fixture fails for the intended seam behavior and retain this RED separately from setup/compiler failures.
- [ ] Step3: Replace only the two throwing seams with counters, reset them with `StartResend`, add the shared ordinary-Tick guard, and register `resend-unexpected-paths` in both existing scenario loops. Assert exact seam counts, zero send calls, false guard and actual branch state differences; all ordinary existing Tick cases must still require zero counts.
- [ ] Step4: Run the actual native Release/Debug/ASan+UBSan matrix (expected28per profile,84total), preserving Apple/non-Apple controls. No uncontrolled socket probe or live game needed. Record commands, source/dependency/output hashes, test count/results and new diagnostics.
- [ ] Step5: Freeze the candidate task-only commit and self-review; yield for independent scoped spec/quality review. Controller qualifies VM1 Win32/x64 Release/Debug under retained policy (expected34per profile,136total), exact source/input/toolchain receipts and preservation checks. Task remains open until the required Windows gate passes; no fresh source edits during immutable VM qualification unless controller admits a scoped repair.

This plan is queued, not dispatched. Its compiler/source admission depends on the current browser task yielding; it makes no current pass or full-migration claim.
