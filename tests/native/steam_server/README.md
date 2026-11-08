These opt-in fixtures build the actual owned Steam service into a reloadable
consumer module. The driver loads the module, runs two secure server sessions,
unloads it, and repeats three times. Each session uses the real
`SteamGameServer015` interface, typed connected/security-policy callbacks, native
high addresses and `SteamGameServerUtils()->GetAppID()` for CS/App10 or HL/App70.
The callback owner is allocated before SDK initialization and destroyed after
SDK shutdown, matching `Steam_Activate` and `Steam_Shutdown`.

```sh
cmake -S tests/native/steam_server -B build/steam-server-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DCSE_STEAM_FIXTURE_PROVIDER=/absolute/path/to/pinned/provider
cmake --build build/steam-server-tests
```

Only separately authorized real-provider execution may run the driver. Set
`SteamAppId` and `SteamGameId` to the tested application ID, then invoke:

```text
steam_server_probe /absolute/path/to/steam_server_owned_module 70
```

Use the target platform's module suffix. `--listen-server-context` as a final
argument also initializes a real licensed client session and keeps it active
across the server sessions. The default server-only path does not initialize or
log in a client. All sessions bind loopback, disable server advertisement and
use the shared-query mode. These fixtures do not run a game server, implement
query packet transport, or qualify gameplay/client authentication/voice.

`CSE_STEAM_SERVER_BUILD_DIRECT_CONTROL=ON` adds a direct SDK executable. On Windows
it also requires `CSE_STEAM_DIRECT_IMPORT_FILE` matching the committed archive
hash. Invoke `steam_server_direct_probe --run-isolated-server-session 70` under
the same application environment. Optional diagnostic modes are
`--single-session-diagnostic`, `--listen-server-context` and
`--retain-anonymous-transport`. Each is explicit; single-session results must not
be presented as repeated-session acceptance.

The pinned Mac SDK's server-only bootstrap can crash during process exit after
repeated initialization; direct SDK controls reproduce it. The production owned
service retains an anonymous communication pipe across server sessions until
module unload. It creates no local game user and performs no client login.
The three flat signatures and `SteamClient020` version come from pinned Valve
[steam_api_flat.h](https://github.com/ValveSoftware/source-sdk-2013/blob/e1f1fd42271805072f052051c26c21caa2c87b04/src/public/steam/steam_api_flat.h)
and [isteamclient.h](https://github.com/ValveSoftware/source-sdk-2013/blob/e1f1fd42271805072f052051c26c21caa2c87b04/src/public/steam/isteamclient.h).
Direct controls can retain the same transport explicitly for comparison. Preserve
both failing controls and final exit/module-unload evidence.

Ordinary builds/CTest never run a real server session. The separate
`tests/native/steam_transport` suite uses controlled exports to check atomic
binding, high opaque pointers, retryable failures, independent owners and final
pipe release, with no real SDK or socket.
