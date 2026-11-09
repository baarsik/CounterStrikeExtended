#include "VGUI_App.h"
#include "VGUI_Scheme.h"
#include "VGUI_TickSignal.h"
#include "controlled_surface.h"
#include "VGui1AppPlatform.h"

#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace platform = cse_vgui1_app;
#include "clock_vectors.h"

namespace {
std::uint32_t clockValue = 100;
bool clockAvailable = true;
unsigned int clockReads = 0;
std::string callOrder;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

struct Context {
    std::thread::id owner = std::this_thread::get_id();
    int retained = 0;
    int retainCalls = 0;
    int releaseCalls = 0;
    int snapshots = 0;
    int snapshotLeases = 0;
    int snapshotReleases = 0;
    int writes = 0;
    int pumps = 0;
    int arenas = 0;
    bool releaseAllowed = true;
    bool snapshotOk = true;
    bool snapshotNullData = false;
    bool closeInsidePump = false;
    bool closeInsideRetain = false;
    bool retainAllowed = true;
    bool closeInsideSnapshotRelease = false;
    bool snapshotNoLease = false;
    bool deferClipboard = false;
    platform::Borrower deferredBorrower{};
    std::uint64_t reportedBytes = 0;
    platform::PumpResult pumpResult = platform::PumpResult::Empty;
    platform::Status nestedClose = platform::Status::Ok;
    std::vector<unsigned char> clipboard = { 'A', 0, 'B', 0, 0, 0 };
    std::map<std::string, std::vector<unsigned char>> settings;
};

bool Owner(void* value) {
    return static_cast<Context*>(value)->owner == std::this_thread::get_id();
}

bool Retain(void* value) {
    Context& context = *static_cast<Context*>(value);
    ++context.retainCalls;
    if (!context.retainAllowed) {
        return false;
    }
    ++context.retained;
    if (context.closeInsideRetain) {
        context.nestedClose = platform::Close();
    }
    return true;
}

bool Release(void* value) {
    Context& context = *static_cast<Context*>(value);
    ++context.releaseCalls;
    if (!context.releaseAllowed) {
        return false;
    }
    --context.retained;
    return true;
}

platform::PumpResult Pump(void* value) {
    Context& context = *static_cast<Context*>(value);
    ++context.pumps;
    callOrder += "P";
    if (context.closeInsidePump) {
        context.nestedClose = platform::Close();
    }
    return context.pumpResult;
}

bool Arena(void* value, int x0, int y0, int x1, int y1, bool enabled) {
    Context& context = *static_cast<Context*>(value);
    ++context.arenas;
    return !enabled || (x0 == 10 && y0 == 20 && x1 == 30 && y1 == 40);
}

bool Acquire(void* value, platform::ClipboardSnapshot* snapshot) {
    Context& context = *static_cast<Context*>(value);
    ++context.snapshots;
    snapshot->data = context.snapshotNullData ? nullptr : context.clipboard.data();
    snapshot->storageBytes = context.reportedBytes ? context.reportedBytes : context.clipboard.size();
    snapshot->lease = context.snapshotNoLease ? nullptr : &context;
    if (snapshot->lease) {
        ++context.snapshotLeases;
    }
    return context.snapshotOk;
}

void ReleaseSnapshot(void* value, void* token) {
    Context& context = *static_cast<Context*>(value);
    Require(token == &context, "snapshot lease identity");
    ++context.snapshotReleases;
    if (context.closeInsideSnapshotRelease) {
        context.nestedClose = platform::Close();
    }
}

bool WriteClipboard(void* value, const unsigned char* data, std::uint64_t bytes) {
    Context& context = *static_cast<Context*>(value);
    ++context.writes;
    context.clipboard.assign(data, data + static_cast<std::size_t>(bytes));
    if (context.deferClipboard) {
        Require(platform::AcquireBorrower(context.deferredBorrower) == platform::Status::Ok,
            "lazy clipboard takes an explicit code/context borrower");
    }
    return true;
}

bool ReadSetting(void* value, const char* key, void* destination, std::uint64_t capacity) {
    Context& context = *static_cast<Context*>(value);
    const auto found = context.settings.find(key);
    if (found == context.settings.end() || (destination && found->second.size() > capacity)) {
        return false;
    }
    if (destination && !found->second.empty()) {
        std::memcpy(destination, found->second.data(), found->second.size());
    }
    return true;
}

bool WriteString(void* value, const char* key, const char* text) {
    Context& context = *static_cast<Context*>(value);
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    context.settings[key] = std::vector<unsigned char>(bytes, bytes + std::strlen(text) + 1);
    return true;
}

bool WriteInteger(void* value, const char* key, std::int32_t number) {
    Context& context = *static_cast<Context*>(value);
    const std::uint32_t encoded = static_cast<std::uint32_t>(number);
    context.settings[key] = {static_cast<unsigned char>(encoded), static_cast<unsigned char>(encoded >> 8),
        static_cast<unsigned char>(encoded >> 16), static_cast<unsigned char>(encoded >> 24)};
    return true;
}

platform::ServicesV1 Services(Context& context) {
    return {sizeof(platform::ServicesV1), platform::ServicesVersion, &context, Owner, Retain, Release,
        Pump, Arena, Acquire, ReleaseSnapshot, WriteClipboard, ReadSetting, WriteString, WriteInteger};
}

class ActualApp : public vgui::App {
public:
    using vgui::App::surfaceBaseCreated;
    using vgui::App::surfaceBaseDeleted;
    void main(int, char**) override {
    }
    ~ActualApp();
    void PumpBase() {
        vgui::App::platTick();
    }
    void SetRunning(bool value) {
        _running = value;
    }
    bool Running() const {
        return _running;
    }
};

// Cleanup only: access the actual Dar allocation using the C++ member operation,
// never a representation cast or a replacement Dar/App declaration.
template<class Tag, class Type, Type Member> struct Access {
    friend auto DataMember(Tag) {
        return Member;
    }
};
struct SurfaceData { friend auto DataMember(SurfaceData); };
using SurfaceDar = vgui::Dar<vgui::SurfaceBase*>;
template struct Access<SurfaceData, decltype(&SurfaceDar::_data), &SurfaceDar::_data>;
struct TickData { friend auto DataMember(TickData); };
using TickDar = vgui::Dar<vgui::TickSignal*>;
template struct Access<TickData, decltype(&TickDar::_data), &TickDar::_data>;

ActualApp::~ActualApp() {
    delete[] (_surfaceBaseDar.*DataMember(SurfaceData{}));
    delete[] (_tickSignalDar.*DataMember(TickData{}));
    vgui::Scheme* scheme = getScheme();
    scheme->vgui::Scheme::~Scheme();
    ::operator delete(scheme);
}

void ClockValues() {
    for (const auto& vector : ClockVectors) {
        std::uint32_t actual = 0;
        Require(platform::EncodeClockTicks(vector.ticks, vector.numerator, vector.denominator, actual)
            && actual == vector.expected, "independent96-bit clock oracle");
    }
    ActualApp app;
    for (std::uint32_t value : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        clockValue = value;
        const long actual = app.getTimeMillis();
        const long long expected = value <= INT32_MAX ? static_cast<long long>(value)
            : static_cast<long long>(value) - 4294967296LL;
        Require(static_cast<long long>(actual) == expected, "signed32 clock result on LP64 and LLP64");
    }
    std::uint32_t output = 77;
    Require(!platform::EncodeClockTicks(1, 1, 0, output) && output == 77, "zero denominator unchanged");
    Require(!platform::EncodeClockTicks(1, 0, 1, output) && output == 77, "zero numerator unchanged");
}

void Clipboard() {
    Context context;
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "bind complete services");
    {
        ActualApp app;
        Require(app.getClipboardTextCount() == 6, "capacity includes NUL and slack");
        for (int offset = -1; offset <= 7; ++offset) {
            for (int capacity = 0; capacity <= 8; ++capacity) {
                unsigned char destination[12];
                std::memset(destination, 0x5a, sizeof(destination));
                const int copied = app.getClipboardText(offset, reinterpret_cast<char*>(destination + 2), capacity);
                const int expected = offset >= 0 && offset < 6 && capacity > 0
                    ? (capacity < 6 - offset ? capacity : 6 - offset) : 0;
                Require(copied == expected, "offset/capacity count oracle");
                for (int index = 0; index < 12; ++index) {
                    const unsigned char byte = index >= 2 && index < 2 + expected
                        ? context.clipboard[static_cast<std::size_t>(offset + index - 2)] : 0x5a;
                    Require(destination[index] == byte, "raw bytes and destination guards");
                }
            }
        }
        char destination[4] = {'x', 'x', 'x', 'x'};
        context.snapshotOk = false;
        Require(app.getClipboardText(0, destination, 4) == 0 && std::memcmp(destination, "xxxx", sizeof(destination)) == 0, "failed snapshot untouched");
        context.snapshotOk = true;
        context.snapshotNullData = true;
        Require(app.getClipboardText(0, destination, 4) == 0 && std::memcmp(destination, "xxxx", sizeof(destination)) == 0, "invalid span untouched");
        context.snapshotNullData = false;
        context.reportedBytes = static_cast<std::uint64_t>(INT_MAX) + 1;
        Require(app.getClipboardTextCount() == 0, "capacity overflow is rejected");
        Require(app.getClipboardText(0, destination, 4) == 0 && std::memcmp(destination, "xxxx", sizeof(destination)) == 0, "overflow never copies");
        context.reportedBytes = INT_MAX;
        Require(app.getClipboardText(INT_MAX, destination, 4) == 0
            && std::memcmp(destination, "xxxx", sizeof(destination)) == 0, "INTMAX offset at capacity untouched");
        context.reportedBytes = 0;
        context.snapshotNoLease = true;
        Require(app.getClipboardText(0, destination, 4) == 0 && std::memcmp(destination, "xxxx", sizeof(destination)) == 0, "unleased span untouched");
        context.snapshotNoLease = false;
        Require(app.getClipboardText(0, nullptr, 4) == 0, "null destination rejected before acquisition");
        Require(app.getClipboardText(0, destination, -1) == 0, "negative capacity rejected");
        context.clipboard.resize(300);
        for (std::size_t index = 0; index < context.clipboard.size(); ++index) {
            context.clipboard[index] = static_cast<unsigned char>(index);
        }
        char paste[256] = {};
        Require(app.getClipboardText(0, paste, sizeof(paste)) == sizeof(paste), "full TextEntry paste capacity");
        Require(std::memcmp(paste, context.clipboard.data(), sizeof(paste)) == 0, "full paste preserves raw bytes");
        context.clipboard.clear();
        context.snapshotNullData = true;
        Require(app.getClipboardTextCount() == 0 && app.getClipboardText(0, destination, 4) == 0,
            "valid leased empty snapshot never copies");
        context.snapshotNullData = false;
        const char raw[] = {'q', 0, static_cast<char>(0xff)};
        app.setClipboardText(raw, 3);
        Require(context.clipboard == std::vector<unsigned char>({'q', 0, 0xff, 0}), "write exact payload and terminator");
        const int writes = context.writes;
        app.setClipboardText(raw, INT_MAX);
        app.setClipboardText(nullptr, 1);
        app.setClipboardText(raw, 0);
        Require(context.writes == writes, "invalid writes never reach backend");
        Require(context.snapshotLeases == context.snapshotReleases, "all snapshot leases released once");
    }
    Require(platform::Close() == platform::Status::Ok && context.retained == 0, "close retained code/context");
}

void Lifecycle() {
    Context context;
    context.closeInsideRetain = true;
    auto services = Services(context);
    services.acquireClipboard = nullptr;
    Require(platform::Bind(services) == platform::Status::InvalidArgument && context.retained == 0,
        "incomplete services cannot claim readiness");
    context.retainAllowed = false;
    Require(platform::Bind(Services(context)) == platform::Status::ReleaseFailed && context.retained == 0,
        "failed retain publishes no callable binding");
    context.retainAllowed = true;
    services = Services(context);
    Require(platform::Bind(services) == platform::Status::Ok, "bind lease");
    services.pumpEvents = nullptr; // Bind owns a value copy, not the caller record's address.
    Require(context.nestedClose == platform::Status::Busy, "retain reentry cannot inspect unpublished callbacks");
    context.closeInsideRetain = false;
    platform::Status workerResult = platform::Status::Ok;
    std::thread worker([&] { workerResult = platform::Close(); });
    worker.join();
    Require(workerResult == platform::Status::WrongThread && platform::Inspect().state == platform::State::Bound,
        "worker close does not begin shutdown");
    platform::Borrower token{};
    Require(platform::AcquireBorrower(token) == platform::Status::Ok, "retain deferred callback");
    Require(platform::Close() == platform::Status::Busy && context.releaseCalls == 0, "borrower blocks release");
    Require(platform::ReleaseBorrower(token) == platform::Status::Ok, "drain deferred callback during closing");
    Require(platform::ReleaseBorrower(token) == platform::Status::InvalidBorrower, "duplicate release cannot decrement");
    context.releaseAllowed = false;
    Require(platform::Close() == platform::Status::ReleaseFailed && context.retained == 1, "failed release retains lease");
    context.releaseAllowed = true;
    Require(platform::Close() == platform::Status::Ok && context.retained == 0, "retry release");
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "rebind new generation");
    Require(platform::ReleaseBorrower(token) == platform::Status::InvalidBorrower, "old generation never releases new owner");
    {
        ActualApp app;
        context.closeInsidePump = true;
        app.PumpBase();
        Require(context.nestedClose == platform::Status::Busy && context.retained == 1, "in-flight callback blocks close");
    }
    Require(platform::Close() == platform::Status::Ok, "complete reentrant close after call unwinds");
}

void BorrowerLimitAndDeferredClipboard() {
    Context context;
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "borrower limit bind");
    platform::Borrower tokens[platform::MaximumBorrowers]{};
    for (auto& token : tokens) {
        Require(platform::AcquireBorrower(token) == platform::Status::Ok, "full bounded borrower ledger");
    }
    platform::Borrower untouched{17, 19};
    Require(platform::AcquireBorrower(untouched) == platform::Status::Exhausted
        && untouched.generation == 17 && untouched.identifier == 19, "overflow token untouched");
    Require(platform::Close() == platform::Status::Busy, "all live tokens hold context");
    for (const auto& token : tokens) {
        Require(platform::ReleaseBorrower(token) == platform::Status::Ok, "drain bounded ledger");
    }
    Require(platform::Close() == platform::Status::Ok, "close drained ledger");
    Require(platform::Close() == platform::Status::Ok, "repeat close releases once");
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "lazy clipboard bind");
    {
        ActualApp app;
        context.deferClipboard = true;
        app.setClipboardText("raw", 3);
        Require(platform::Close() == platform::Status::Busy && context.retained == 1,
            "lazy clipboard must retire before code/context release");
        Require(context.clipboard == std::vector<unsigned char>({'r', 'a', 'w', 0}),
            "lazy clipboard owns a copy after App staging allocation expires");
        Require(platform::ReleaseBorrower(context.deferredBorrower) == platform::Status::Ok,
            "retire lazy clipboard payload");
    }
    Require(platform::Close() == platform::Status::Ok, "close after lazy retirement");
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "snapshot release reentry bind");
    {
        ActualApp app;
        context.closeInsideSnapshotRelease = true;
        Require(app.getClipboardTextCount() == 4, "snapshot survives through release callback");
        Require(context.nestedClose == platform::Status::Busy && context.retained == 1,
            "snapshot lease release runs while service code is pinned");
    }
    Require(platform::Close() == platform::Status::Ok, "finish snapshot reentry close");
}

void ActualSurfaceAndTickOrder() {
    Context context;
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "surface/tick bind");
    clockValue = 100;
    clockReads = 0;
    callOrder.clear();
    ActualApp app;
    Require(clockReads == 1 && callOrder == "C", "base constructor reads cold clock before UI binding use");
    {
        ControlledSurface first;
        ControlledSurface second;
        app.surfaceBaseCreated(&first);
        app.surfaceBaseCreated(&second);
        int x = 0;
        int y = 0;
        app.getCursorPos(x, y);
        Require(x == -123 && y == 456 && first.reads == 1 && second.reads == 0,
            "actual App first-Surface virtual dispatch and signed coordinates");
        app.surfaceBaseDeleted(&first);
        app.surfaceBaseDeleted(&second);
    }
    struct Signal : vgui::TickSignal {
        void ticked() override {
            callOrder += "T";
        }
    } signal;
    app.addTickSignal(&signal);
    clockValue = 99;
    callOrder.clear();
    app.externalTick();
    Require(callOrder == "C", "early shared tick reads clock once without pump/signal");
    clockValue = 100;
    callOrder.clear();
    app.externalTick();
    Require(callOrder == "CPTC", "eligible actual shared tick reads clock then pump/signal/final clock");
    clockValue = 149;
    callOrder.clear();
    app.externalTick();
    Require(callOrder == "C", "shared tick honors scheduled interval");
    clockValue = 150;
    callOrder.clear();
    app.externalTick();
    Require(callOrder == "CPTC", "shared tick schedules from completion read");
    Require(platform::Close() == platform::Status::Ok, "surface/tick close");
}

void RegistryAndPump() {
    Context context;
    Require(platform::Bind(Services(context)) == platform::Status::Ok, "bind");
    {
        ActualApp app;
        Require(vgui::App::getInstance() == &app, "actual static instance");
        Require(app.setRegistryString("Software\\CSE\\value", "abc"), "string delegate");
        char text[8] = {};
        Require(app.getRegistryString("Software\\CSE\\value", text, 8) && std::strcmp(text, "abc") == 0,
            "string bytes and capacity");
        Require(app.getRegistryString("Software\\CSE\\value", nullptr, 0), "query only preserves backend result");
        Require(app.setRegistryInteger("integer", -123), "int32 delegate");
        int number = 0;
        Require(app.getRegistryInteger("integer", number) && number == -123, "four-byte integer");
        context.settings["partial"] = {0x41, 0};
        number = 0x12345678;
        Require(app.getRegistryInteger("partial", number) && number == 0x12340041, "partial query preserves high bytes");
        app.internalSetMouseArena(10, 20, 30, 40, true);
        Require(context.arenas == 1, "arena dispatch");
        app.SetRunning(true);
        context.pumpResult = platform::PumpResult::Quit;
        app.PumpBase();
        Require(app.Running(), "legacy quit returns without inventing stop");
        context.pumpResult = platform::PumpResult::Error;
        app.PumpBase();
        Require(!app.Running(), "pump error stops");
        app.SetRunning(true);
        context.pumpResult = static_cast<platform::PumpResult>(999);
        app.PumpBase();
        Require(!app.Running() && platform::Inspect().failure == platform::Failure::BackendFailure,
            "unknown event result cannot report successful readiness");
        app.setCursorPos(99, 100);
        Require(context.arenas == 1, "measured no-warp operation has no invented callback");
    }
    Require(platform::Close() == platform::Status::Ok, "close");
}
}

namespace cse_vgui1_app {
bool ReadColdClock(std::uint32_t& encoded) noexcept {
    ++clockReads;
    callOrder += "C";
    encoded = clockValue;
    return clockAvailable;
}
}

int main(int argc, char** argv) {
#if defined(_WIN32)
    // Abort children must remain headless; diagnostics are printed by production.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const std::string scenario = argc > 1 ? argv[1] : "all";
    if (scenario == "fatal-clock") {
        clockAvailable = false;
        ActualApp app;
        return 91;
    }
    if (scenario == "fatal-unbound") {
        ActualApp app;
        return app.getClipboardTextCount();
    }
    if (scenario == "fatal-worker" || scenario == "fatal-closing") {
        Context context;
        Require(platform::Bind(Services(context)) == platform::Status::Ok, "fatal fixture bind");
        ActualApp app;
        if (scenario == "fatal-closing") {
            platform::Borrower token{};
            platform::AcquireBorrower(token);
            platform::Close();
            return app.getClipboardTextCount();
        }
        std::thread worker([&] { app.getClipboardTextCount(); });
        worker.join();
        return 92;
    }
    ClockValues();
    Clipboard();
    Lifecycle();
    BorrowerLimitAndDeferredClipboard();
    ActualSurfaceAndTickOrder();
    RegistryAndPump();
    std::printf("PASS actual App platform fake-service component\n");
    return 0;
}
