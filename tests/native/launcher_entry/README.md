# Portable launcher entry fixture

Builds the actual four launcher translation units selected by the owning retained
manifest. The fixture starts the executable in an empty temporary directory and
checks malformed argv rejection before any game/module startup. Oversized input
must exit with a normal error status and diagnostic; the exact 4095-byte payload
boundary must proceed to the controlled missing-filesystem path.

Configure this directory with Ninja, Release or Debug, and macOS deployment
minimum14.0. `CSE_LAUNCHER_ENTRY_SANITIZERS=ON` instruments the complete executable.
Run CTest afterward. This fixture neither starts the game nor qualifies real
engine/provider startup, module packaging, locale presentation or a macOS bundle.
