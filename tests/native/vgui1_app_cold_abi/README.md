# VGUI1 AppCold compiler foundation

This folder is build-time/component evidence. The separate AppCold admission
fixture covers the new unselected constant publisher and file reader. This foundation
changes no App declaration, implementation, build manifest, loader or module.
Runtime10, Descriptor288 and the16-table limit stay unchanged. No probe is linked
or executed.

`cse_vgui1_app_cold_abi.h` uses the actual declaring App header included by its
caller. Its explicit-instantiation access tags preserve private/protected member
types via `decltype`; they do not rewrite access specifiers, create a substitute
App class, decode member-pointer representations or invoke them. All types,
array extents, qualifiers, storage sizes/alignment and signedness come from the
compiler. Only the two measured signed32 timing fields normalize int/long.

`tools/vgui1_app_cold_evidence_schema.json` maps25 App storage fields, both real
Dar instantiations' three fields, the static borrowed instance pointer, the
imported static getter and55 full virtual method identities. Compiler reports
supply offsets, vptr/base/constructor facts, actual slot numbers and overload
identity. Semantic signatures bind identities **and** full types/physical
positions; schema renumbering cannot hide same-type field/method reorder.
Report framing, compiler/path/source/include hashes and options are freshness
provenance, not ABI compatibility. Ownership revision describes engine static
storage, provider construction/allocation and borrowed-pointer lifetime.

Run local controls with an actual Apple owner database:

```sh
CSE_VGUI1_COLD_DATABASE=/absolute/build/compile_commands.json \
python3 -m unittest discover -s tests/native/vgui1_app_cold_abi -p 'test_*.py'
```

The controls reuse actual engine vgui_intwrap.cpp and provider App.cpp recipes.
For a positive local control only, the real private provider header is selected
in both unchanged CPPs; this is clearly a controlled binding. The current copied public/private header pair now matches; a historical
negative control recreates the four former Apple registry return differences
only in copied headers and requires rejection. Neither copied-header control is
producer metadata evidence; the new metadata mode requires fresh exact actual
hw/sw/common-App database entries and the real declaring headers. The retained MSVC report fixture is captured compiler text; it tests
only parser handling of mid-line `/showIncludes` diagnostics.

A normal evidence invocation selects actual compiler database entries:

```sh
python3 tools/vgui1_app_cold_evidence.py generate \
  --schema tools/vgui1_app_cold_evidence_schema.json \
  --foundation src/public/cse_vgui1_app_cold_abi.h \
  --contexts /absolute/app-owner-contexts.json --output /absolute/evidence
```

The context manifest has format1, `compile_database` pointing to the actual
compiler database, and `contexts` entries with `name`, `role` (`consumer` or
`provider`), `target` and absolute `source`. Consumers must be the real
vgui_intwrap.cpp containing CDummyApp; the provider is the real App.cpp. Explicit
argument manifests require `--fixture-only` and cannot become publisher evidence.
Every invocation revokes prior acceptance, preprocesses and compiles every
context, checks all inputs/closures/compiler identities again and fails closed on
missing/unknown reports or drift. There is no cached or manually pinned fallback.

Matching evidence emits a guarded three-family Catalog (role/profile/AppCold),
`receipt.json`, per-context facts and `accepted.stamp`. The header requires
`CSE_VGUI1_APP_COLD_COMPONENT_EVIDENCE`; `PublisherReady` and `ColdLoadReady` are
always false. Real Apple disagreement emits observations/failure and removes
both accepted stamp/header. IDs32–35 correspond to slots31–34. The no-arg App base
constructor is measured; App(bool), other VGUI1 public classes and later
backend/gameplay behavior are outside this AppCold catalog. Static getter type
is covered because its import is resolved at engine load; its later use and
backend implementation are not runtime-qualified by this component.
