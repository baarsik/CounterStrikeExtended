# Common POSIX argv encoding

Build this directory with Ninja, then run CTest. The existing launcher test driver
links the complete common command-line implementation and its own actual public
header. Only the three argv scenarios execute: legacy quoting/separators, exact
4095-byte limits and overflow rejection without replacing the current command.
Use `CSE_COMMANDLINE_SANITIZERS=ON` for ASan/UBSan. No response file or provider is
used. This does not qualify the other common parser/mutator methods or a launcher.
