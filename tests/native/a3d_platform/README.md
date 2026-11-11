# A3D platform declaration fixtures

These CPU fixtures compile the actual retained A3D headers. `layout_tests` catches
LP64 widening, misplaced fields, global packing leaks and truncated pointer-bearing
records. `interface_tests` calls every slot of 28 complete COM, DirectSound and
A3D views through frozen C caller tables, using test-only marker implementations
of the production interfaces. The slot tables are independent checked-in
contracts, not generated from production headers during configuration or testing.
SDK records and complete DirectSound slot orders were checked against Windows SDK
10.0.26100.0 headers and independent x86/x64 probe executions (probe SHA256
`8bcfbadc453c179e39bd3c01fdd6da7fc4f1c8a3b8ee6724bfa7754d180cd10d`).
The retained A3D contracts were frozen from revision
`01ea4f52c5b8713ddd6d5381a5b2479775770c65`.

The multiple-view identity fixture checks `this` adjustment and the typed native
GUID pointer lane with real high-address storage. Geometry checks distinguish
public tags from full-width polygon identities. These declaration tests do not
qualify the production provider's QueryInterface behavior, output, scheduling,
notifications, OEM activation or any game consumer.

Run from the repository root, one compiler at a time:

```sh
cmake -S tests/native/a3d_platform -B build/a3d-native-20261111/platform-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/a3d-native-20261111/platform-release --parallel 1
ctest --test-dir build/a3d-native-20261111/platform-release --parallel 1 --output-on-failure
cmake -S tests/native/a3d_platform -B build/a3d-native-20261111/platform-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCSE_TARGET_ARCH=arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build/a3d-native-20261111/platform-debug --parallel 1
ctest --test-dir build/a3d-native-20261111/platform-debug --parallel 1 --output-on-failure
```

The fixture has no SDL or external dependency. It links the actual A3D GUID unit;
Windows uses the actual SDK headers and `uuid`/`dxguid` libraries. The independent
`windows_sdk_probe.cpp` includes only SDK headers and prints record sizes,
alignments and offsets. Build it from an x86 or x64 Native Tools environment with
`cl /nologo /W4 /WX /EHsc /std:c++17 /Od windows_sdk_probe.cpp /Fe:sdk_probe.exe`,
then run `sdk_probe.exe`. This does not open an audio device or activate COM.

The owning target's genuine Release/Debug `compile_commands.json` inventories
belong in a new `build/a3d-native-20261111/graph-*` directory, configured from the
runtime presets with the controller's existing pinned dependency paths. Inventory
all 16 ordinary and five generated geometry translation units, preserving the
ordered command options (later options override earlier ones). A fixture pass is
not a pass for the actual module: OS calls and LP64 arithmetic remain separately
owned porting work until the real target builds and its runtime is qualified.
