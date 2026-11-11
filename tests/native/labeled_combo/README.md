# Labeled-command combo selection

```sh
python3 tests/native/labeled_combo/test_selection.py --root . --evidence build/labeled-combo
```

Compiles the actual class declaration, selection/item/reset methods and production CUtlVector
with its real assertions under ASan/UBSan. The UI service deliberately issues nonconsecutive
item IDs, keeping item IDs distinct from the class's row indices. The fixture constructor
sets up controlled UI services; it does not run a real VGUI window.

Cases cover negative and upper-bound indices, mapping row selection to item IDs, selection
invalidation after deleting all items, reset and applied engine commands. The pre-fix negative
index reproduces the real viewdemo dialog's vector assertion. Real game viewdemo acceptance
is a separate runtime test, not implied by this component fixture.
