# Engine bootstrap ABI foundation

This is a metadata/component packet. It adds no module publisher, loader gate,
target, export, game or provider execution. The new public header uses the
current runtime10 / 288-byte descriptor. It is **not sufficient for publication
without the mandatory owning-build slot/freshness gate** described in the artifact
report. Production build integration is separate.

`prepare_bindings.py` pins exact declarations from the real engine, launcher and
dedicated sources. Isolated namespaces permit inspection of the existing mutable
producer / const-spelled caller declarations in one controlled fixture. These
bindings are labeled; they are not substitutes for real owner-TU qualification.

`source_probes.py` includes the unchanged real owner/caller CPP and appends typed
calls, real base conversions and metadata checks under an actual Ninja compiler
recipe. It never links or runs those production objects. `declaration_probes.py`
uses pinned declarations where current full Mac owners remain blocked by Windows
headers. Its reordered/inserted virtual controls preserve class size and still
pass named-member type validation.

`check_slots.py` independently checks physical compiler vtables. Every compile
must succeed, every expected interface must appear exactly once, and physical
destructor/method order must match. `--stamp` first removes an earlier accepted
stamp and writes one only after a complete positive check. A negative control
never leaves accepted evidence. This demonstrates a fail-closed prerequisite;
it is not complete CMake dependency/code-generation wiring.

`contract_test.cpp` and `run_component.py` exercise the typed declarations and a
small in-process callback model. Missing/wrong role, profile, shape, envelope,
count and filesystem closure reject before modeled callbacks. Positive controls
check virtual calls, factory roles, borrowed inputs and status pointees. The
filesystem result is a marker that is never used as IFileSystem. Its nested ABI
is the existing qualified catalog plus actual compiled virtual-call probes.
No real filesystem provider, engine, dedicated server, socket or module loader
is executed.

Representative local commands (complete pinned commands are in artifact receipts):

```sh
python3 tests/native/engine_bootstrap_abi/prepare_bindings.py \
  --root "$PWD" --output "$ART/engine-bootstrap-abi-v1/bindings"
PYTHONDONTWRITEBYTECODE=1 python3 tests/native/engine_bootstrap_abi/declaration_probes.py \
  --root "$PWD" --database "$ART/macos-runtime-policy-release/compile_commands.json" \
  --bindings "$ART/engine-bootstrap-abi-v1/bindings/actual_declarations.h" \
  --output "$ART/engine-bootstrap-abi-v1/current-slots"
python3 tests/native/engine_bootstrap_abi/check_slots.py \
  --directory "$ART/engine-bootstrap-abi-v1/current-slots" --profile itanium \
  --stamp "$ART/engine-bootstrap-abi-v1/current-slot-stamp.json"
```

The packet distinguishes actual Win32/x64 retained owner recipes, real ARM64
compiler controls reusing native x64 per-file policies, native actual launcher
compilation, controlled declaration probes, and standalone C++14 model runs.
They are not interchangeable whole-runtime acceptance receipts.
