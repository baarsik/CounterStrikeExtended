# Unchanged Win32 server fixture

`lock.json` pins the official ReHLDS 3.15.0.896, Metamod-R 1.3.0.149 and AMX Mod X
1.9.0.5303 base/Counter-Strike release archives and every PE member. These are
external reference applications, rebuilt neither by this project nor for native
clients. `verify_server_fixture.py` verifies hashes, archive paths and x86 machine
types without executing code. Acquire the named archives from their locked URLs
into a separate cache and run:

```sh
python3 tools/qualification/verify_server_fixture.py --archive-directory /path/to/cache --output /path/to/new-report.json
```

Use a separate **pre-anniversary HLDS `steam_legacy` base**, as required by the
[ReHLDS installation instructions](https://rehlds.dev/docs/rehlds/install/):

```text
steamcmd +force_install_dir <isolated-base> +login anonymous +app_set_config 90 mod cstrike +app_update 90 -beta steam_legacy validate +quit
```

A current Half-Life game installation supplies a different Steam API DLL and is
not a valid substitute: the initial fixture attempt could not load ReHLDS because
`SteamApps`, `SteamGameServer_Init` and `SteamGameServer` exports were absent.
Preserve the legacy base's depot manifest and file hashes with fixture evidence.
Never make source or binary compatibility changes to the reference server.

Overlay the locked Windows ReHLDS files into the base, install Metamod-R under
`cstrike/addons/metamod`, and the AMXX base/CS packages under `cstrike`. Configure
`liblist.gam` to load `addons/metamod/metamod.dll`, and Metamod's `plugins.ini` to
load `win32 addons/amxmodx/dlls/amxmodx_mm.dll`. Compile `cse_compat_probe.sma` using
the pinned AMXX compiler/includes and enable that plugin in AMXX's configuration.
The probe requires the unchanged AMXX fakemeta module.

The plugin exercises a menu/reply, HUD/chat messages, a custom unhooked user-message
boundary, cvar queries, sound emission, downloads and model consistency. Prepare
`sound/cse_fixture/probe.wav` and `cse_fixture/probe.txt` in `cstrike` before starting
the server; record their hashes. It also checks exact `models/w_c4.mdl` consistency.
Bind the server only to an isolated loopback endpoint, e.g. `127.0.0.1:27438`.
Preserve authentication and integrity policy (`sv_lan 0`, `mp_consistency 1`);
`-insecure` is not an acceptance workaround.

After real client connection, locally issue `cse_fixture_start`, select the first
menu item, then issue `cse_fixture_ack`. Correlate plugin markers with client demos,
module hashes, screenshots/audio, the received resource hashes and actual server
state. Sending a HUD or sound on the server alone does not prove client delivery.
The custom message intentionally has no client hook: later messages and the final
ack must still work. Native clients and unchanged external server binaries never
exchange in-process interfaces.

Archive verification, fixture setup, plugin compilation and server startup are
separate evidence classes. None alone qualifies authentication, gameplay or
external-server compatibility. The fixture is not yet a complete automated
acceptance suite; the active migration plan retains that gate as open.
