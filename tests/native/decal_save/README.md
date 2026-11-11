# Client decal save records

This fixture compiles the actual `CL_Save` and `CL_Restore` bodies with bounded
in-memory filesystem and renderer callbacks. It checks exact legacy bytes,
writer/reader round trips, every truncated record length, excessive counts, and
invalid entity indices. The record stays 32 bytes, including its 16-byte name,
on Win32 and native 64-bit hosts. Pointer size never determines disk width.

Configure/build this directory, then run CTest. Full game save/load and
cross-target save acceptance remain separate runtime checks.
