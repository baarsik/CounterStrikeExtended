# HLTV NetSocket native boundary checks

These fixtures compile the complete actual NetSocket/Network/BaseSystemModule, ObjectList, TokenLine,
BitBuffer, NetAddress, common and byteorder implementations. Typed fake socket/DNS functions, controlled RNG,
standalone interface-registry head storage, and real-interface System/channel subclasses are explicit test
bindings. No replacement production class declaration, extracted algorithm, real socket/DNS lookup, provider,
game, GUI or user's terminal is used. Network::Init is not called; its startup/DNS boundaries abort if reached.

The suite covers exact direct/padded/split UDP bytes and nine-byte split headers, ordered reassembly, queues,
packet deep copy/free/Flush, channel matching/readiness order, zero/short/error counts, RNG short-circuit order,
all four affected overflow call sites, socket creation failures at every step, fd0, live-Create no-mutation,
repeat Close/reused descriptors, channel/packet cleanup, cleanup errno, typed native options and legacy TTL
text/range handling. Close-failure controls model a descriptor consumed by close despite EINTR and prohibit
unsafe retries. They do not establish the host kernel's descriptor state after every possible close failure.

From repository root:

```sh
ART=/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/hltv-socket-v1
python3 tests/native/hltv_socket/run_mac.py --recipes "$ART/actual-tu-red.json" --output "$ART/recheck"
python3 tests/native/hltv_socket/compile_tus.py --recipes "$ART/actual-tu-red.json" --output "$ART/tus-recheck"
python3 tests/native/hltv_socket/verify_windows.py --before "$ART/before/GoldSrc-SDK/HLTV/Core/NetSocket.cpp" \
  --output "$ART/selection-recheck.json"
```

Negative runtime control adds `--before --source "$ART/before/GoldSrc-SDK/HLTV/Core/NetSocket.cpp"`.
It retains visible original conversion warnings while omitting Werror gates only for that broken runtime
source. Strict original full-TU REDs are separately retained. Positive components use C++14 and the owning
warning/FP/exception profile; complete actual Mac TUs keep the retained C++17 recipe.

`stage_windows.py` freezes the real include/source closure. `run_windows.py` builds the frozen original
Windows-selected NetSocket implementation with typed fake calls in x86/x64/ARM64 Release/Debug environments;
it also compiles the current full production TU unhooked. `compare_reference.py` checks all common-case bytes,
results and traces against the actual frozen MSVC Win32 Release component. Only CRLF is normalized. The
Windows source-selection proof is distinct from actual MSVC compilation/execution and from Mac-only policies.

Only existing Core/NetSocket.cpp is production membership; headers/layout, split algorithms, sealed Console
and object-reset packets, and build policies/manifests are unchanged. Actual UDP/netchan interoperability,
full module initialization, malformed split validation, sequence/static-assembly state and byte-statistic
integer overflow remain outside this qualification. Whole-runtime integration and promotion are separate.
