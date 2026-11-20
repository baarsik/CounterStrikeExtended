# Expat 2.8.5

The unmodified source release archive comes from [libexpat](https://github.com/libexpat/libexpat/releases/tag/R_2_8_5).
CMake checks its SHA-256 against the published release digest before extracting it. The standalone AMXX module and native forms tests use a static library with DTD and custom general entity support disabled. The game client uses the runtime's pinned Expat provider and rejects DTD declarations from its parser callback.
