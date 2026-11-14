# Slowhacking filter

Run from the repository root:

```sh
python3 tests/native/slowhacking_filter/run.py
```

The runner extracts the production tokenizer, command buffers, dispatcher,
stufftext validation, CVAR permission checks and engine default. ASan/UBSan
cases cover all three levels, trusted local commands, mixed case, quoted names,
alias expansion and split buffers. Command effects and CVAR writes are observed
at their external boundary without opening a game or network connection.

A second executable uses the actual GameUI option declarations, initialization,
list ownership, value transfer and configuration serialization. It checks CS
and HL, new/missing scripts, duplicate/stale entries, final position, all three
choices and saved CVAR preferences. Filesystem and widget services are controlled;
these checks do not validate an interactive game window or rendered text fit.

The maintained translation resources use their existing suite:

```sh
python3 -m unittest discover -s tests/localization_resources -v
```
