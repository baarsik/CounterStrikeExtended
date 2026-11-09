# Character-set byte domain

The fixture compiles the actual CharacterSetBuild implementation. It verifies
all255 nonzero byte values, guards around the table, duplicate ASCII delimiters,
empty strings and null arguments. Release/Debug profiles are explicit; optional
ASan/UBSan uses CSE_ENABLE_SANITIZERS=ON. The preserved original fails both the
character-index compiler check and the exhaustive byte/guard check.
