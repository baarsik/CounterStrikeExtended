# Maintaining the CMake build

CMake is the source of truth for the active runtime build. Edit the checked-in
target definitions directly. Historical upstream projects remain available only
as [archived provenance](msbuild/README.md).

## Build ownership

| Location | Owns |
| --- | --- |
| `CMakeLists.txt` | Runtime composition and feature selection |
| `CMakePresets.json` | Supported toolchain/configuration entry points and default output roots |
| `cmake/targets/<target>/CMakeLists.txt` | Target kind, output name/path, compile/link settings, libraries, exports, resources, assets and generated inputs |
| `cmake/targets/<target>/Sources.cmake` | Source membership, configuration exclusions and platform replacement sources |
| `cmake/RuntimeTargets.cmake` and dependency modules | Shared runtime, dependency, staging, export and generated-input helpers |
| `cmake/MSVCDefaults.cmake` | MSVC configuration defaults policy |

Use the root `hardware-x86-release` or `hardware-x86-debug` presets for
hardware-only builds with the same target definitions.

Keep source properties in the directory that creates the owning target. A shared
source can have different forced includes, definitions, optimization, FP or EH
settings in different targets. Preserve that separation when moving settings or
adding sources. Use explicit configuration and platform conditions for changes
that apply only to Debug, Release, Windows, macOS, POSIX or a particular CPU.

The target definitions retain CRT selection, assertions, include order, forced
headers, exclusions, resources, export names/ordinals and per-file FP/EH settings.
The MSVC defaults hook clears generic CMake configuration flags before target
settings are applied. Explicit cache flag overrides remain the caller's
responsibility. Release symbols and linker maps are gated by
`CSE_RELEASE_DIAGNOSTICS`; Release incremental linking is disabled.

Native SDL3 integration, Windows libraries used with 64-bit pointers and guarded
POSIX module aliases are CMake target policies. Add platform replacement sources
in `Sources.cmake` and matching platform flags/dependencies in `CMakeLists.txt`.
Inspect platform-specific source selections with the
[CMake inventory tool](#inspecting-effective-target-settings).
Counter-Strike keeps the `mp` output name and provides the `cs.dylib` / `cs.so`
alias on POSIX. Alias creation must not replace an unrelated file.

## Generated production inputs

Run the source generator after changing its CMake-owned input inventories or
the shared declarations used by its ABI layouts:

```sh
python3 tools/generate_runtime_sources.py
```

Review derived source changes together with those inputs. This command generates
`src/common/generated_game_module_layouts.h`,
`src/common/generated_client_module_layouts.h`, server
`generated_saved_callbacks` fragments and native Windows export projections
under `cmake/exports/`. It does not regenerate target definitions.

The original Windows x86 module-definition files retain their bytes and basenames
under `cmake/exports/win32/`; other source-owned `.def` files remain with their
implementations. Keep exports and architecture conditions in the owning target
definition synchronized with the appropriate export file.

VGUI1 rebuilds its Windows x86 compatibility import archive from the names-only
`src/utils/vgui/vgui_imports.def` after linking `vgui.dll`.

The reference A3D build has 16 ordinary compile inputs and five generated geometry
translation units. `cse_a3d_geometry_source` creates unique wrappers for OBJECTS,
FINALIZE, ACOUSTICS, SUBMISSION and STATE sections of `A3dGeom.cpp`, alongside CORE.
The wrappers live in the target build directory and retain their per-file
settings in `cmake/targets/a3dapi/CMakeLists.txt`. This preserves the upstream
`PrepareGeometryCompilation` behavior without requiring upstream build projects.

## Inspecting effective target settings

Produce an inventory for the platform, CPU and configuration under review:

```sh
python3 tools/cmake_inventory.py --target-os windows --architecture x86 \
  --configuration Release --output build/runtime-inventory.json
```

The tool uses CMake File API metadata from a host-compiler configure with symbolic
external dependencies. Records describe targets, runtime outputs, sources and
effective CMake compiler settings for the selected configuration. Supported
metadata selectors are `windows`, `macos` or `linux`; `x86`, `x64` or `arm64`; and
`Debug` or `Release`. A selector does not establish support for building that
runtime; see [supported platforms](building.md#platforms-and-dependencies).

Python consumers import these functions from `tools/cmake_inventory.py`:

```python
query_targets(root=None, configuration="Release", target_os="windows", architecture="x86", diagnostics=False)
query_target(root, target, configuration="Release", target_os="windows", architecture="x86", diagnostics=False)
```

`root` is an optional repository path for `query_targets`; `query_target` requires
the root argument (pass `None` for the current repository) and a target name.
Both inspect the same CMake-owned definitions as the CLI. Add `--diagnostics`
to the CLI, or use `diagnostics=True`, to include opt-in Release diagnostics.

Per-source `compile_options` contains the evaluated options declared by CMake;
host-injected compiler defaults are excluded. Language standards and target
visibility/PIC settings are reported separately in `properties`. Generated A3D
wrappers carry a `generated` descriptor with their section and original source;
their temporary query paths are not persistent files. Fixtures recreate those
wrappers in their own build directory. Objective-C++ source metadata is inspected
through host C++ generation, without compiling the source.

Metadata inspection does not cross-compile, validate a target toolchain, check
provider binaries or establish runtime behavior. Build with the real platform
preset for compiler/linker validation, and inspect built exports, imports,
resources and architecture separately.

The Windows runtime binary audit requires an explicitly supplied CMake-derived inventory:

```sh
python3 tools/qualification/audit_runtime_inventory.py /absolute/game-runtime \
  --inventory build/runtime-inventory.json --architecture x86 \
  --output /absolute/evidence/runtime-inventory.json
```

Match inventory OS, CPU and configuration to the supplied runtime. Static import
and binary inventories remain separate evidence from actual OS/provider loading,
gameplay and device checks. See [qualification tools](../tools/qualification/README.md).

## Reviewing build changes

Review target settings and source membership together, regenerate affected
production source files, and inspect inventories for the affected configurations
and platforms. Run the relevant tooling tests and real build presets:

```sh
python3 -m unittest discover -s tools/tests
```

Keep dependency notices, pinned upstream bytes and provider provenance intact.
CMake-owned selections and generated inventories do not change the rights of
the selected source; see [licensing](licensing.md).
