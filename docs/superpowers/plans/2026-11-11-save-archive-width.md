# Fixed-width Save Archive Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Restore the260-byte JSAV archive contract while safely reading existing native1024-byte saves.

**Architecture:** A private checked archive owner centralizes full-layout validation, deterministic writing and extraction. Both existing read sites and the writer use it; existing save header/entity/global algorithms remain unchanged.

**Tech Stack:** Existing C++17 engine, FS_* byte-oriented functions, CMake/Ninja, real-source CPU fixture and current owning production compiler recipes.

**Spec:** docs/superpowers/specs/2026-11-11-save-archive-width-design.md

## Global Constraints

- All new archives use exactly260 filename bytes, a four-byte little-endian nonnegative length, and unchanged payload bytes.
- Readers support complete legacy260 and prior unreleased native1024 entry layouts on every updated target.
- Preserve JSAV version0x71 and existing header/token/global/entity algorithms.
- Validate the whole declared archive through exact EOF before creating or truncating extracted outputs; reject ambiguous distinct nonempty layouts.
- Preserve original input saves and their bytes; no bulk migration/deletion/rewriting.
- FS_Read/FS_Write return bytes; FS_Seek returns void. Verify exact counts and resulting positions.
- Preserve production TU selection/compiler policy, Root index/HEAD, all prior A3D freezes; no broad warning suppression or speculative unrelated fixes.
- One production/test source owner and Mac1; Root Windows1; global compiler cap2, every build/test/nested helper serial1. No device/VM/UI/SDK/protection/route/service/dependency actions by source worker.

## Review Focus

- Native1024 padding may be nonzero after NUL: compatible complete records must still decode.
- Zero/negative/impossible count, short reads, negative/oversized length, failed seek and trailing bytes: reject without extracted outputs.
- Both layouts structurally valid: reject ambiguous nonempty input, do not pick a host-dependent winner.
- Traversal/absolute/drive/tilde names or unterminated fields: reject without filesystem output or input mutation.
- Partial writer opens/reads/writes or count mismatch: return save failure, never successful partial save.

## Task 1: Checked canonical writer and compatible archive readers

**Files:** Create GoldSrc-SDK/engine/save_archive_impl.h (private real-FS owner), tests/native/save_archive/{CMakeLists.txt,archive_tests.cpp,fixture.h,extract.py,README.md}; modify GoldSrc-SDK/engine/host_cmd.cpp only necessary archive integration. Existing engine-manifest selection stays unchanged unless actual evidence requires a narrow generated dependency fix.

**Interfaces:** No new public ABI. Private namespace cse::save_archive defines constexpr kLegacyNameBytes=260, kPriorNativeNameBytes=1024, kLengthBytes=4, and `struct Layout { std::uint32_t start; std::uint32_t end; std::int32_t fileCount; std::uint32_t nameBytes; };`. Exact private operations: `bool ReadLayout(FileHandle_t file, std::int32_t fileCount, Layout &layout);`, `bool Extract(FileHandle_t file, const Layout &layout);`, `bool WriteDirectory(const char *pattern, FileHandle_t file, std::int32_t expectedFileCount);`. ReadLayout validates and leaves cursor at archive start; failure clears layout. Extract revalidates caller-supplied layout before output. Use existing Host_SaveGameDirectory/FS leaves through private header as in save_arena; no fake production IO. Host_Load and DirectoryExtract call common validation/extraction. SaveGameSlot calls the checked writer and propagates its result. Preserve any retained wrapper names/signatures if a concrete external callsite requires them; document focused lookup.

- [ ] Write real-source RED fixture showing old Mac actual DirectoryCopy emits filename1024 and fails fixed260 expected bytes, while old host-width reader cannot decode a source-pinned260 multi-entry archive. Fixtures use short heap input strings under ASan to detect padding overread. The source-extraction helper pins SHA/anchors and rejects missing/duplicate anchors; it does not supply a replacement archive algorithm.
- [ ] Run only new failing fixture in fresh save-archive-red build; record actual nonzero output and exact commands. Do not touch frozen A3D build directories.
- [ ] Implement the three private operations, bounded canonical zero-filled records, checked little-endian length/count/seek/copy behavior, and actual Host_Load/DirectoryExtract/DirectoryCopy/SaveGameSlot integration. Keep C++4space/braces/descriptive constants and edits scoped.
- [ ] Exercise canonical260 byte equality under fixture host limits260 and1024, multi-entry payloads including zero-byte entries, both260/1024 reads with nonzero old padding, exact EOF, zero-entry archive, long terminated/unterminated names, malformed counts/lengths/seek/truncation/trailing/ambiguous layouts, unsafe names and injected write/open/read/copy failure. Assert original bytes unchanged, no outputs for validation failures, failure return for partial writer. Independent expected bytes are manually encoded fixed-width fixture data, not produced by the codec under test.
- [ ] Configure/build/test new Release/Debug fixtures serially, plus scoped ASan/UBSan for short-string and malformed bounds. Required command shape: cmake -S tests/native/save_archive -B build/save-archive-20261111/{release,debug,sanitize} -G Ninja -DCMAKE_BUILD_TYPE=<profile>; cmake --build <dir> --parallel1; ctest --test-dir <dir> --parallel1 --output-on-failure. Use actual two argv values `--parallel 1`. Sanitizer flag is named in fixture README; no old graphs or global defaults changed.
- [ ] Configure new actual root Mac normal-preset graphs with existing pinned dependencies and all retained targets; compile only actual hw_engine/swds host_cmd.cpp object recipes Release/Debug under their generated ordered flags. Capture DB/output/commands; don't run full19 twice. Repeat affected existing save_arena/save_fields/decal_save once only if actual changed seams require them, record why and results. No general suite discovery.
- [ ] Freeze scoped source, final successful commands/logs and artefacts; record all previous A3D3093 artifacts unchanged. Preserve all failed receipts/logs with unique names. Commit only scoped WT files; release source/Mac1, provide report for independent review/integration.

## Task 2: Controller cross-target and real save/load acceptance

**Files:** New owned controller evidence/packets under build/save-archive-20261111, and validation status after evidence. No product source edits in this task without a new scoped source fix dispatch.

**Interfaces:** Consumes reviewed Task1 commit, real-source CPU fixture and fixed disk contract. Produces actual Win32/x64 fixture output byte hashes and coherent-client save/load roundtrip receipts, separate from component tests.

- [ ] Prepare/review false-admitted fresh normal-user VM1 fixture packet for actual MSVC x86/x64 Release/Debug compilation/run of the same archive owner tests, pinned tools/SDK/environment/serial budget/protected inputs and exact owned-child supervision. Compare literal canonical bytes and prior-native1024 extraction results across targets. No provider/runtime/network installation or fixture service changes.
- [ ] Root runs one admitted bounded packet; preserve failed attempts and fix source only through original owner/scoped rereview. Compile success alone is not a format runtime pass.
- [ ] After later full19 coherent Mac builds/staging, prepare owned Half-Life SP save/load capture with real maxplayers1/skill2 startup/spserver hook sequencing, genuine initialized client, finite commands and original assets/saves read-only. Verify actual typed player/map/entity/weapon state in saved data and after load, archive layout260, actual server/client loading/normal lifecycle. Mark test-only item grants explicitly; no initialization masking. Cross-target full save payload/campaign acceptance needs actual evidence, not just archive extraction.
- [ ] Update status with precise passed/unrun/failed boundaries. Do not block unrelated completed A3D software status on physical hardware; do not promote same-build/dummy or component results to full migration.

## Self-review and handoff

Spec coverage maps both read sites, writer, error propagation and native compatibility toTask1; cross-target actual execution and coherent game roundtrip toTask2. ReviewFocus five classes have explicit assertions. All private signatures are fixed above and no wire/public interface is added. User full clearance and explicit no-more-answers opt-out authorize the implementation method already in use; no further approval/question is pending. Controller dispatches one source owner only after Task7 independent integration.
