# Bootstrap compiler evidence pipeline

This packet adds a standalone generator, semantic schema, CMake helper and scoped
controls. It does not edit retained targets, manifests, loaders or publishers.
The prior bootstrap foundation remains frozen.

Run the bounded controls with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/native/engine_bootstrap_evidence/test_pipeline.py
```

`CSE_EVIDENCE_TEST_ARTIFACTS=/absolute/new/directory` retains each test's exact
sources, contexts, commands, compiler reports and generated results. The controls
compile only fixture objects and never load an owned runtime module or execute
an engine/provider. Image identity controls hash owned fixture bytes; they do not
parse a PE/Mach-O image or claim real constructor safety.

The generator consumes `format: 1` context manifests. Each context names its role
(`engine`, `launcher`, `dedicated`), actual source, cwd, compiler arguments/command
and actual qualified filesystem header. A manifest may instead supply a real
`compile_database` and target/source selectors. Missing or ambiguous entries fail;
there is no fabricated default compiler profile. Explicit constructor requirements
are mandatory.

The compiler replay keeps effective ABI options and replaces only private
source/output/dependency destinations. It expands bounded response files, records
their contents, resolves compiler/frontend identities and captures only relevant
compiler option/include environment inputs. Every validation preprocesses the
actual contexts again, including cached builds: old dependency hashes alone cannot
detect a newly shadowing header. Pre/post hashes reject inputs changing during
replay. Whole-environment dumps and incidental compiler/source hashes do not enter
runtime compatibility.

Actual compiled type validators, semantic selector mapping and compiler-reported
slot/count/base/adjustor facts must all agree. The generated header's `Bind` mixes
actual physical facts with the locally typed descriptor, retaining runtime10 and
the 288-byte envelope. Schema-only edits cannot excuse an actual reorder while
the frozen typed schema disagrees. There is no manual slot fallback.

`cse_add_engine_bootstrap_evidence` creates an always-validated hard prerequisite,
forces the generated header into C++ publisher compilation and preserves a no-op
object build when accepted inputs remain unchanged. Compiler, schema, flag,
response/include drift or unknown reports remove acceptance. This helper is not
hooked into any retained target in this packet.

**Production mode intentionally fails closed.** Actual engine-owned VGUI1
constructor requirements, their catalog and a reviewed pre-load identity reader
are not implemented. `--fixture-only` / `FIXTURE_ONLY` results always contain
`publisher_ready=false` and `cold_load_ready=false`; their generated headers reject
ordinary publisher consumption. A matching launcher expectation is insufficient:
the identity model separately compares the engine/consumer's own requirement with
the staged provider, then checks exact fixture image hashes. Exported getters
would be compared after safe loading; a late getter never protects initializers.

Real integration must still provide the reviewed constructor-edge solution,
scope any metadata-only probe exclusion needed to break a publisher dependency
cycle, and wire the runtime targets/publishers to consume generated bound metadata.
No current full-runtime or ABI-completeness promotion follows from this packet.
