These standalone fixtures cover localization registry IDs, string-pool offsets and the CS HUD font ID.
They build outside the repository and do not start a game, provider or UI.

From the repository root:

```sh
python3 tests/native/vgui_registry_ids/run_checks.py --phase green --output /tmp/cse-vgui-registry-ids
```

The runner uses C++14 and tests Release, Debug and ASan/UBSan configurations. It compiles both independent
`ILocalize` declarations and the CS forwarding header with both public include-directory orders. It checks
the actual method signatures, unsigned 32-bit representation and invalid sentinels. The actual HUD header
is compiled independently; eight font-ID patterns pass through its field type and the actual engine callback type.

The container fixture imports the private production record/tree/vector types through test-only access.
It uses the localization provider's actual include profile; the interface-header probes separately vary both
public include-directory orders. CS has different containers that cannot be mixed with the provider's definitions.
It extracts six table methods and seven historical adapter methods from `LocalizedStringTable.cpp`, recording
the source and individual method hashes. Table method qualifiers bind to a small controlled class that uses
the real containers; the historical adapter receives controlled modern-table responses. These bindings avoid
unrelated filesystem/surface services. The complete production class and its virtual dispatch are not exercised.

The checks cover boundary ID patterns, the `0xffffffff` invalid ID, name-byte and wchar-element offsets, real
tree insertion/search/traversal and native allocations above 4 GiB. Extracted methods preserve null-name,
case-insensitive `#` lookup, invalid-value and traversal behavior. The legacy adapter still truncates returned
IDs to 16 bits and forwards incoming `0xffff` as `65535` without silently changing it to the modern invalid ID.
Large boundary IDs are checked as stored values; the fixture does not attempt billion-element allocations.

Optional `--actual-tu-commands path.json` replays saved production compiler arguments with only object outputs
redirected to the requested artifact directory. This is distinct from the standalone component compilation.

`windows_type_probe.cpp` additionally proves the exact Windows `unsigned long` API/field/tree types, localization
record layout and selected method signatures. The qualification packet compiles and runs this probe plus the
three actual interface-header probes against frozen before/after header closures using real MSVC x86, x64 and
ARM64, in Release and Debug. Windows selected-token checks are a separate preprocessing check: the tree alias
spelling intentionally changes, while its exact Windows template type remains the same.

The packet is component and TU evidence. It does not qualify a full runtime, external module ABI or gameplay.
