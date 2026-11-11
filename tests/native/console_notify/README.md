# Actual console notify owner

`extract.py` copies the complete `Con_DrawNotify()` function and its actual
`g_szNotifyAreaString` declaration byte for byte from `console.cpp`. Named
rendering/time/chat services record visible glyphs, positions, fonts, text and
bookkeeping; the fixture changes no production body or loop. Source, function,
declaration and include-file hashes are retained separately.

```sh
python3 tests/native/console_notify/run.py --source GoldSrc-SDK/engine/console.cpp \
  --packet build/console-notify-green-unique
```

The runner requires a new packet directory. It snapshots the source and records
commands, exits, logs, compiler command databases and extraction pins for all 20
cases under Release, Debug and ASan+UBSan. No sanitizer checks are disabled.

To retain RED evidence, use `--expect-red` with the original source. A stale
suffix case at 238 columns fails its visible-prefix assertion without exceeding
storage; a fully populated 256-byte buffer at 318 columns fails ASan-only with a
global-buffer-overflow and ASan+UBSan with an out-of-bounds error. The full buffer
at 238 columns passes as a baseline control. The sanitizer profiles are separate
so the combined run's earlier UBSan failure does not hide the ASan diagnostic.

For Windows MSVC fixture checks, configure this independent CMake project from
matching x86/x64 developer shells, then build and run CTest in fresh Release and
Debug directories. `/W4 /WX` remains enabled. The Python runner's sanitizer phase
requires a compiler supporting ASan+UBSan, so use the direct CMake commands for
MSVC. These service-seam tests do not qualify a real font renderer or gameplay.

The existing `../gameplay_shortcuts/build_engine_packet.py` supports optional
`--predecessor <latest-packet> --owners console.cpp`. It retains the original
recipe baseline, verifies the admitted links/pins and current source/header
closure, recompiles both console owners, links the exact predecessor HW/SW
closure, and regenerates the launcher's image cohort. It leaves default owner
selection unchanged. Provider images, apps and live game observations are
outside this fixture.
