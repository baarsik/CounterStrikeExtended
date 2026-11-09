# Actual launcher command-line fixtures

These standalone C++14 tests link the production `launcher/common/commandline.cpp`
and exercise it only through the public `ICommandLine` interface and `cmdline`
singleton. No parser methods or ctype implementations are supplied by the fixture.

```sh
cmake -S tests/native/launcher_commandline -B build-commandline -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCSE_COMMANDLINE_SANITIZERS=ON
cmake --build build-commandline --parallel
ctest --test-dir build-commandline --output-on-failure
```

Cases cover all nonzero byte values in the POSIX whitespace path, ASCII bytes in
the unchanged Windows ctype path, quoted values, response-file expansion and
truncation, Remove/Append/Set, integer values, POSIX argv spacing and quoting,
and exact/overflow boundaries of the 4096-byte buffer. Windows high-bit bytes
are not fed to its retained signed-char ctype expression.

The parser's legacy response-file cursor behavior and space-based quoted-value
extraction are deliberately pinned. POSIX argv assembly quotes only arguments
containing literal spaces, retains empty-argument spacing and embedded quote
bytes, and allows 4095 encoded payload bytes plus the terminator. Larger encoded
argv inputs throw `std::length_error("Command line exceeds 4095 bytes")` before
the current command changes. The caller is responsible for reporting that error.
Raw-string and response-file truncation behavior remains unchanged.

On Windows, `-DCSE_COMMANDLINE_REFERENCE=<preserved-commandline.cpp>` builds the
original parser separately and compares six actual result transcripts with the
edited parser. This is a headless component test; it launches no game/provider.

Generated commands, response files, and binaries stay within the selected build
directory or temporary comparison directories. No default runtime target or root
CMake membership is added.
