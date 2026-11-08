Controlled, offline tests for the production `ServerTransport` owner. These build
small fake export libraries and never load a Valve provider, create a real SDK
session or open a socket.

```sh
cmake -S tests/native/steam_transport -B build/steam-transport-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/steam-transport-tests
ctest --test-dir build/steam-transport-tests --output-on-failure
```

Run both configurations and actual target architectures. The incomplete provider
aborts if its create function is called: all three C exports must resolve before
any factory/creation call. Cases cover missing interfaces, failed creation,
retryable failed release, idempotence, destructor cleanup, two owners and a peer
that keeps the SDK alive when one owner's global-shutdown attempt returns false.
