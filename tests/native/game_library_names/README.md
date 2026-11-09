These fixtures cover native game-library filename adaptation and the selected
`LoadEntityDLLs` metadata branch. The loader fixture extracts the actual source
through library selection, using controlled parser, virtual-filesystem and
module-loader seams. It creates no sockets and loads no game module. The parser
seam accepts the quoted/unquoted fixture values; it does not qualify `COM_Parse`.

The fixture compiles both macOS and Linux branches on the host. It checks native
metadata precedence regardless of key order, Windows-only metadata fallback,
`-dll`, existing illegal-path rules, virtual-filesystem resolution failure,
fallback capacity and the missing-listing wildcard. The separate name fixture
also runs on Windows to check bounded normalization without platform execution.

```sh
cmake -S tests/native/game_library_names -B build/game-library-names -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/game-library-names
ctest --test-dir build/game-library-names --output-on-failure
```

Use `-DCSE_GAME_LIBRARY_SANITIZERS=ON` for ASan/UBSan. Actual native module ABI,
VFS search-order acceptance and full game startup have separate gates.
