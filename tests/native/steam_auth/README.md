The packet fixture compiles intact definitions from `engine/cl_main.cpp`,
`engine/sv_steam3.cpp` and `engine/crc.cpp`, plus the whole info-string
implementation. It substitutes only the surrounding engine services and supplies
an ASCII userinfo and deterministic host identity. It checks the legacy protocol
48 header/ticket boundary, host/network byte order, cached retries, server identity
changes, disconnect and failed/negative/oversized SDK ticket lengths. Mock
results test the production call sites; they do not qualify the provider ABI.

```sh
cmake -S tests/native/steam_auth -B build/steam-auth-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/steam-auth-tests
ctest --test-dir build/steam-auth-tests --output-on-failure
```

With `-DCSE_STEAM_FIXTURE_PROVIDER=/absolute/path/to/pinned/provider`, the build
also produces `steam_auth_server_probe`. It uses the actual owned SDK service,
checks a logged-in licensed CS/App10 session and high native interface addresses,
and calls the same extracted production connection and termination methods.
Only separately authorized explicit execution may open the real session/socket:

```text
SteamAppId=10 SteamGameId=10 steam_auth_server_probe --connect-isolated-win32-loopback
```

The peer is fixed to the unchanged secure Win32 fixture at `127.0.0.1:27438`.
Correlate a fresh server `STEAM USERID validated` line with each invocation and
producing binary hash; the client connection response alone is insufficient.
The driver observes SDK denials for fifteen seconds and does not record, hash or
persist authentication bytes. This does not implement netchan sign-on, gameplay,
voice, consistency/download tests, or AMXX message reception. Ordinary CTest
never loads a real provider or opens a socket. Use both Release and Debug.
