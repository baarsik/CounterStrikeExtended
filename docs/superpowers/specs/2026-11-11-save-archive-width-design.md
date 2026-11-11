# Fixed-width save archive compatibility

## Intent and evidence

Native clients must preserve disk encodings across updated Win32, Windows x64 and Apple Silicon. The user requested completing the migration and explicitly opted out of further questions. Controller choice: keep the legacy writer format and support previously produced native saves; no user approval or answer is outstanding.

`GoldSrc-SDK/engine/host_cmd.cpp` currently writes a host MAX_PATH filename followed by a signed32 payload length and payload in each JSAV0x71 archive entry. Windows uses260 bytes; the actual owning Mac preprocessor proves1024. DirectoryCopy writes directly from a find-name pointer. Both Host_Load inline extraction and DirectoryExtract read the host-sized field. Root finding and actual macro receipt are under build/macos-runtime-bringup-20261109/save-archive-width-finding-20261111.json. FS_Read/FS_Write return BYTES, despite their retained count argument; FS_Seek returns void, FS_Tell/FS_Size unsigned32.

## Chosen behavior and alternatives

All new archives use exactly260 filename bytes, a four-byte little-endian nonnegative length, and unchanged payload bytes. Filename padding is deterministic zero, copied from bounded terminated input. Preserve JSAV version0x71 and existing header/token/global/entity algorithms.

Readers support complete legacy260 and prior unreleased native1024 entry layouts on every updated target. Determine one layout for the whole archive by bounded validation from the current archive start and declared entry count through exact EOF, before creating or truncating extracted outputs. Zero entries are valid only for an empty remaining archive, canonically260. If both distinct nonempty layouts validate, reject ambiguity. Never guess based on the host, only first record, zero padding, or an unchecked seek. Legacy padding after the first NUL need not be zero. Preserve original input saves and their bytes; loading never migrates/reformats/deletes an input save.

A version bump/new format would split interoperability unnecessarily. Writing260 without1024 compatibility would discard progress from the already exercised native build. Keeping host width violates the explicit disk contract. The chosen repair is bounded compatibility reading plus one stable writer.

## Bounds and failure

Use named constexpr domain sizes260/1024/four-byte length, fixed-width scalar types, checked counts/positions and exact byte read/write results. Negative counts/lengths, missing terminators, truncated fields/payloads, impossible extents/counts, seek failure, trailing data and ambiguous layout fail safely. Do not attempt signed absolute seeks outside the existing FS API domain; report failure rather than narrow an offset. Treat count0 explicitly. Keep the existing valid relative map-state filename family (*.HL?) and reject traversal, absolute/drive/tilde paths and unsafe names before any extracted output. Do not reinterpret a malformed archive as a different layout merely to accept it.

The writer must avoid copying padding beyond the actual input string and propagate open/read/write/count/copy failure to SaveGameSlot rather than claiming a successful save. Preserve current slot-age semantics; do not add a deletion, bulk migration or rewrite of existing saves. No speculative general filesystem rewrite, other entity codecs, gameplay, renderer, audio or default-provider edits.

## Structure and ownership

One private engine helper header may follow save_arena_impl.h's real-FS-owner pattern; it centralizes validation/copy/extraction. Host_Load and DirectoryExtract consume the same validated layout. DirectoryCopy/SaveGameSlot produce only the canonical layout and propagate failure. Use focused private wrappers if necessary, with no new public module ABI. Keep existing production TU selection and per-file compiler policy; no new handcrafted substitute engine project.

CPU fixtures compile the real helper and affected directory bodies with labelled in-memory FS leaves. Old source's actual host-width writer/reader must reproduce the discrepancy; assertions inspect bytes and extracted payloads, not mirrored implementation arithmetic. Actual Host_Load campaign/global/entity reconstruction remains a later coherent-engine runtime gate. Meaningful source-owner compilation covers hw_engine/swds under real Release/Debug policies. Windows actual fixture and full-runtime execution are separate controller gates after source review.

## Acceptance and retained limits

New output is byte-identical260 layout across host-width configurations;260 and1024 multi-entry inputs decode to identical named payloads. Malformed/ambiguous/fault cases create no extracted outputs and never modify inputs. All original frozen A3D artifacts remain immutable. No same-build roundtrip alone closes cross-platform compatibility. Preserve user-directed microphone-consent document/defer exception; it is unrelated. Production Metal, physical input/audio and broader save entity/campaign roundtrips remain separate, explicitly unpassed gates.
