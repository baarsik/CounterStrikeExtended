# Native system-module boundary components

This project compiles complete production Core and DemoPlayer translation units into
separate shared libraries without RTTI or LTO. It also compiles the canonical base
implementation and extracts complete current loader/cache methods at configure/build
time. `extract_loader.py` records each extracted region's SHA256; the Console variant
renames only its owning class to the controlled fixture class. File/library services,
engine rendering calls and console output are controlled fixture bindings. The engine
and Console acquisition/removal implementations, registry, listener implementation,
Core Server/World/Network classes, and DemoPlayer implementation are production code.

Tests cover include-root permutations, mandatory provider/consumer admission from a
separate fixture image, adjusted primary/secondary queries, unsupported names, real
DemoPlayer->Server/World initialization, unique serials, command dispatch, World signals,
pointer-based borrowed listeners, self-removal, nested dispatch, failure rollback and
retry, child-first/parent-first/direct/repeated removal, callback shutdown deferral,
borrowed object survival and the engine DemoPlayer cache reset. Failure injection uses
controlled system services, the engine command allocator, and a rejected provider
subscription. On macOS, `failure_listener_allocation` additionally interposes the actual
`realloc` used by BorrowedListeners: initial insertion and growth fail without changing
membership, failed subscriptions receive no callbacks, and real DemoPlayer acquisition
returns null before a successful retry and teardown. The captured allocation stack must
include the real DemoPlayer image. The test uses no replacement production listener
implementation or production allocation hook. `CSE_LISTENER_INTERPOSE_DISABLE=1` is a
negative control: this case must fail its first allocation-result assertion.

The interposer is test-owned and single-threaded. Direct inline-storage checks compile
with `-fno-builtin-realloc` so Clang cannot fold `realloc(nullptr, size)` into `malloc`;
provider compile flags are unchanged. Under AppleClang ASan the test environment inserts
the ASan runtime before the interposer. A poisoned-redzone assertion verifies that
successful forwarded allocations still use ASan. The allocator case is macOS-specific;
it is not a Windows allocator qualification. Snapshot malloc and every STL allocation
site are not exhaustively faulted.

Exception-policy qualification matters: standalone component fixtures (including the
extracted loader) use AppleClang's default enabled exceptions and explicitly disable
RTTI. The owning macOS Release/Debug hw_engine and swds SystemWrapper/BaseSystemModule
TUs instead use `-fno-exceptions -frtti`; Core, DemoPlayer, GameUI, HLTV and Proxy use
`-fexceptions -frtti`. The controlled service `bad_alloc` fixtures therefore demonstrate
rollback only when exceptions are enabled. They do not establish production engine
recovery from throwing services or STL OOM. Registry's Init catch is conditional on
exception support and does not cover every registry allocation. Listener allocation
injection returns null without throwing and exercises the explicit boolean failure path.
No owning runtime exception or allocation policy is changed by these tests.

`frozen/` retains exact pre-repair production engine method regions. The negative
control runs those methods in a separate process and verifies missing macOS engine
registration, duplicate nested serials, and failed-Init success. Real Core legacy-factory
and secondary-view pointer comparisons demonstrate the unadjusted-pointer defect
without dereferencing the incorrectly adjusted base.

```sh
cmake -S tests/native/system_modules -B build/system-modules-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/system-modules-release --parallel 4
ctest --test-dir build/system-modules-release --output-on-failure
```

Use Debug for the ordinary debug run. For ASan+UBSan add
`-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`, the matching
C flags, and `-fsanitize=address,undefined` in both executable/shared linker flags.
Run with `UBSAN_OPTIONS=halt_on_error=1`.

On the tested AppleClang 21 runtime, `lifecycle_image_reload` fails in ASan's globals
registration (a registration crash or duplicate-global/ODR report) on a subsequent load of an instrumented image. It remains an enabled test.
The process-isolated scenarios exercise the same lifecycle and same-registry module
reinitialization but cannot qualify same-process sanitized image reload. The independent
`reproduce_sanitizer_reload.py OUTPUT_DIRECTORY` creates a DSO with 1,500 plain arrays
and no project code and reproduces the same globals-registration failure. Its complete
output is retained in `OUTPUT_DIRECTORY/result.log`. Ordinary Release/Debug unload and
reload pass. Do not describe the full sanitizer suite as passing.

MSVC test configuration supports `/GR-`; native cases require a 64-bit process. Win32
builds only the legacy declaration check. Windows exports/vtable/runtime qualification,
physical GameUI lifecycle, full-game demo playback, Steam, networking and protocol-48
interoperability require separate validation; these tests do not establish them.

`module_name_copy` calls the complete production BaseSystemModule::Init through the
existing lifecycle fixture. It poisons the previous name, then verifies byte prefixes,
termination and all remaining zero padding for null, empty, short, embedded-NUL,
exact-boundary and truncated names. A nonterminated input contains exactly the maximum
readable prefix; ASan can detect an extra read. These are preservation cases for replacing
CRT-deprecated strncpy under the retained Windows DemoPlayer warning policy. Their
ordinary macOS success does not establish MSVC owning-policy acceptance.

The registry regressions run through both extracted engine and Console loaders. They
reactivate a real allocated Core Server, reject a conflicting provider, and count its
original provider Release callback exactly once. Self-removal during Init and parent
reactivation during child cleanup must be rejected until the active frame unwinds;
ordinary retries then succeed.

The running-order, find-order and frame-order modes require newest successful
publication first after nested Init and reactivation. The instance-find and
instance-acquire modes require case variants of an instance name to reuse the same
real Core Server. Both loaders use their owning FindModule/GetModule methods;
RunFrame is extracted from the engine in both fixtures, so the Console's complete
frame loop is not exercised by these component cases.

`fakeclient_cleanup_{success,failure}` compiles the complete production FakeClient TU.
It uses complete extracted Network CreateSocket/GetSocket/RemoveSocket/ShutDown methods
(with original-region hashes), renaming the owner and substituting a controlled socket
service. Thus allocation/list ownership/close/delete decisions are production source,
while OS socket creation and Close are counters, with no UDP handle or network traffic.
Normal removal and failed initialization after socket creation must retain the borrowed
socket in Network's ownership list and close/destroy it exactly once at provider shutdown.
The real Core Server/World source supplies successful children and the controlled
exception-enabled Server failure. This test does not qualify physical socket behavior
or Windows: it is enabled only on non-Windows native component builds. Socket lifetime
is deliberately bounded by Network shutdown, not by FakeClient removal.

The connected teardown modes additionally compile complete Proxy, ProxyClient,
BaseClient, Master, Status, DemoClient and Director translation units, and use the
real NetChannel implementation from Core. Controlled sockets maintain channel lists
and call each real channel's Close during socket shutdown. Tests leave a spectator or
FakeClient's Server channel attached when whole-system Stop or explicit Proxy removal
begins; explicit Network removal is covered too. They require channel owners to detach
before socket Close, one channel removal, exactly-once provider socket destruction,
and no surviving client/Proxy registration. Network shutdown observers verify that
new subscriptions and socket creation are rejected during notification. Registry tests
check reverse successful-publication order after nested Init and reactivation.

The only controlled external services are socket I/O, a filesystem accepting search-path
setup (unexpected filesystem operations abort), and absent Steam service exports. No
Steam provider, UDP handle or remote protocol handshake is used. FakeClient's real
Server::Connect binds its real channel and enters the challenge state; the tests qualify
that cleanup path, not a completed game-server handshake. The connected fixtures remain
non-Windows; Windows and live acceptance are controller-owned.
