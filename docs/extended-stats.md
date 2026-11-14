# Extended health and armor

CSE supports signed 32-bit `HealthEx` and `BatteryEx` values and displays up to
99999 health and armor. The optional `cse_extstats` AMXX module provides automatic
observation and explicit Pawn natives on **Win32 and Linux32** Counter-Strike and
Condition Zero servers. Half-Life is outside this extension's scope.

## Build and install

Use CMake 3.25+, Ninja and an x86 MSVC environment on Windows:

```sh
cmake --preset amxx-win32-release
cmake --build --preset amxx-win32-release
```

On Linux, install a C++17 compiler and its 32-bit multilib development libraries:

```sh
cmake --preset amxx-linux32-release
cmake --build --preset amxx-linux32-release
```

These presets build the module independently of the CSE runtime. A 64-bit module
is rejected. Each build writes `build/<preset>/package/`, containing the module,
config, Pawn include/example source and SDK license notices. Copy its
`addons/amxmodx/` tree into the server's game directory. Keep the accompanying
license notices with redistributed packages. AMXX and Metamod must already be
installed. The module uses the pinned AMXX **1.9.0.5303** SDK and standard engine
interfaces, without ReAPI or private player offsets. Runtime qualification on
1.9/1.10 is a separate check from the SDK build.

Add this line to `addons/amxmodx/configs/modules.ini`:

```text
cse_extstats
```

Restart the server or change maps after installation/removal. Confirm `amxx modules`
lists the module as running. Automatic mode needs no companion AMXX plugin. Compile
`scripting/example.sma` using the installed AMXX compiler to try `/cse_stats` in chat;
the example gives the caller 99999 health and armor. Use existing CStrike APIs to
grant kevlar/helmets; changing an amount preserves the current equipment type.

The module executes `addons/amxmodx/configs/cse_extstats.cfg` (respecting
`amxx_configsdir`) at each map start before observation. Its default is:

```text
cse_extstats_auto 1
```

Changes take effect next frame. Setting 0 disables ordinary numeric observation;
plugins must synchronize damage/healing/armor changes themselves. Connection,
death/respawn, HUD resets and spectator-target changes still synchronize. Enabling
observation again forces current state. HLTV's periodic snapshots repeat published
cached values, so they do not secretly observe numeric changes in manual mode.

## Pawn API

```pawn
#include <cse_extstats>

native cse_set_user_health(index, health);
native cse_set_user_armor(index, armor);
native cse_sync_user_stats(index);
native bool:cse_is_supported(index);
```

Setters and synchronization return 1 on success and raise native errors for invalid
or disconnected players. Bots can be stat subjects. Nonpositive health invokes the
game DLL's normal client-kill path; positive health does not respawn dead players.
Negative armor is an error. Synchronization republishes actual entity fields after
changes made through another API. Calls from message hooks queue transmission until
the current engine message ends.

`cse_is_supported(index)` reports recognized **CSE identity**, independently of Ex
protocol support. Invalid/disconnected clients, bots, HLTV proxies, unanswered or
failed detection return false. Detection queries `cse_client=1` and separately
`cse_extstats_protocol=1`, expires after two seconds, and rejects stale connection
responses. CSE builds predating these markers cannot be reliably identified. These
markers express client capability; they are not authentication or an anti-cheat check.

## Values and compatibility

Entity health/armor remain game-owned **floats**, with the existing damage and
equipment behavior. Every integer through 99999 is exactly representable. Above
16777216, adjacent integers are not all representable: setters publish their actual
float readback rather than an independent integer shadow. Conversion truncates
positive fractions, keeps living fractional health at least 1, maps NaN/nonpositive
values to 0, and saturates infinity/large values before conversion to `INT32_MAX`.
Dead players publish health 0. Client state retains received 32-bit amounts; only
rendering caps them to 99999.

The unchanged legacy messages accompany Ex updates: health is bounded to 255,
armor to 999, and packed HLTV health to 127 while retaining FOV/flag behavior.
Entity amounts are never capped for fallback transmission. Stock clients retain
these safe limited displays. Updated clients work on servers without the module
using their legacy messages. Accurate larger values require the server extension.
Existing AMXX message hooks remain in the publication path and can block/modify
messages; an original game message and the module's canonical fallback can both
invoke callbacks. Plugins overriding these messages must preserve the Ex contract
or deliberately block it. Do not register the Ex names for another payload: an
existing incompatible registration disables module publication and logs the reason.

Both messages have exactly **5 bytes**: unsigned player index, then a signed
little-endian 32-bit amount (`WRITE_BYTE`, `WRITE_LONG`). Values 0..2147483647 are
valid; -1 invalidates that player's stat, other negatives are rejected. Registrations
are resolved by name. Legacy message formats, prediction deltas and shared viewport
interfaces are unchanged.

Direct Ex packets go only to negotiated clients for their own stats or their
server-selected spectator target. Active players receive no new opponent stats;
scoreboard visibility rules remain unchanged. Health and known armor appear in the
spectator label; unknown armor is omitted. The local sprite HUD keeps three-position
alignment through 999 and grows to five positions, moving armor right as needed.

The module also sends changes and full cached snapshots every second through
`MSG_SPEC`, including bots. Existing HLTV relays can carry the registered user
messages. Stable late joins and demo resets wait for the next snapshot on the
broadcast timeline, plus the relay's configured delay and network time. There is
no immediate relay state injection. Backward demo time clears client Ex state;
legacy fallbacks remain until the next snapshot.

## Verification

The standalone production-code fixtures cover conversion/wire boundaries, identity,
manual/automatic observation, lifecycle, routing, message registration and hooks,
legacy precedence, sprite digits/layout and localized spectator formatting:

```sh
cmake -S tests/native/extended_stats -B build/tests/extended-stats -G Ninja
cmake --build build/tests/extended-stats
ctest --test-dir build/tests/extended-stats --output-on-failure
```

`tools/qualification/extended_stats_probe.py` prepares a controlled test plugin and
analyzes its runtime log into JSON. Use `--help` for compiler, optional RCON/client/
relay orchestration and evidence paths. Its result distinguishes binary inspection,
module/native loading, observed server-side payloads and remaining client/HLTV
visual acceptance. A green build or server callback log is not a screenshot or proof
of downstream delivery. Run the live probe only on an owned test server: it creates
a bot and changes its health/armor through the boundary sequence. Inspect 99999
health and armor together at 640x480 and larger resolutions, death/respawn, stock
clients, ordinary spectators, HLTV late joins and recorded-demo seeking before rollout.
