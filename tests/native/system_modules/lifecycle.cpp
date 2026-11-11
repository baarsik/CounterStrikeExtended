#include "fixture.hpp"
#include <array>
#include <algorithm>
#if defined(LISTENER_ALLOCATOR_INTERPOSITION)
#include "listener_allocator.h"
#if defined(LISTENER_ALLOCATOR_ASAN)
#include <sanitizer/asan_interface.h>
#endif
#endif
bool failCommandAllocation = false;
extern "C" void* Mem_ZeroMalloc(std::size_t size) {
    if (failCommandAllocation && size == sizeof(SystemWrapper::command_t)) { return nullptr; }
    return std::calloc(1, size);
}
SystemWrapper gSystemWrapper;
SystemWrapper* currentModuleSystem = nullptr;
int COM_BuildNumber() { return 1; }

class Listener : public BaseSystemModule {
public:
    int calls = 0;
    Listener* publisher = nullptr;
    Listener* remove = nullptr;
    bool nested = false;
    bool shutdown = false;
    void Emit() { FireSignal(1, nullptr); }
    void ReceiveSignal(ISystemModule*, unsigned int, void*) override {
        ++calls;
        if (remove) { publisher->RemoveListener(remove); }
        if (nested) { nested = false; publisher->Emit(); }
        if (shutdown) { m_System->ShutDown(); }
    }
};

#if defined(LISTENER_ALLOCATOR_INTERPOSITION)
static void ListenerAllocationFailure(SystemWrapper &system) {
    constexpr std::size_t InitialCapacity = 8;
    constexpr std::size_t GrowthCapacity = InitialCapacity * 2;
    constexpr int AllocationPublisherSerial = 100;
    Listener publisher;
    std::array<Listener, InitialCapacity + 1> listeners;
    publisher.Init(&system, AllocationPublisherSerial, const_cast<char *>("allocation-publisher"));
    BorrowedListeners storage;
    ListenerAllocatorArm(nullptr, InitialCapacity * sizeof(ISystemModule *));
    Require(!storage.Add(&listeners[0]), "initial allocation failure returned true");
    Require(ListenerAllocatorFailures() == 1, "initial realloc was not faulted");
    Require(storage.begin() == storage.end() && !storage.Contains(&listeners[0]), "failed insert changed membership");
    for (std::size_t index = 0; index < InitialCapacity; ++index) {
        Require(storage.Add(&listeners[index]), "allocation retry failed");
    }
#if defined(LISTENER_ALLOCATOR_ASAN)
    Require(__asan_address_is_poisoned(storage.end()), "interposer bypassed ASan allocation redzone");
#endif
    auto *entries = storage.begin();
    ListenerAllocatorArm(entries, GrowthCapacity * sizeof(ISystemModule *));
    Require(!storage.Add(&listeners.back()), "growth allocation failure returned true");
    Require(ListenerAllocatorFailures() == 1 && storage.begin() == entries, "growth did not preserve allocation");
    Require(storage.end() - storage.begin() == InitialCapacity, "growth changed member count");
    for (std::size_t index = 0; index < InitialCapacity; ++index) {
        Require(storage.begin()[index] == &listeners[index], "growth changed member order");
    }
    Require(!storage.Contains(&listeners.back()), "failed growth added listener");
    Require(storage.Add(&listeners.back()), "growth retry failed");
    storage.clear();
    Require(storage.begin() == storage.end(), "listener cleanup failed");

    ListenerAllocatorArm(nullptr, InitialCapacity * sizeof(ISystemModule *));
    Require(!cse::modules::Subscribe(&publisher, &listeners[0]), "failed subscription returned true");
    Require(ListenerAllocatorFailures() == 1, "subscription did not reach realloc");
    publisher.Emit();
    Require(listeners[0].calls == 0, "failed subscription left stale listener");
    Require(cse::modules::Subscribe(&publisher, &listeners[0]), "subscription retry failed");
    publisher.Emit();
    Require(listeners[0].calls == 1, "retried subscription did not receive signal");
    publisher.RemoveListener(&listeners[0]);
    publisher.Emit();
    Require(listeners[0].calls == 1, "removed subscription still received signal");
    publisher.ShutDown();
}
#endif

static void Admission() {
    static cse::modules::Factory factory{cse::modules::Contract, sizeof(cse::modules::Factory),
        cse::modules::ContractRevision, [](const char*) -> ISystemModule* { std::abort(); },
        [](ISystemModule*) { std::abort(); }};
    auto lookup = +[](const char* name, int*) -> void* {
        if (cse::modules::Is(name, cse::modules::FactoryName)) { return &factory; }
        if (cse::modules::Is(name, cse::modules::ConsumerName)) { return &factory.descriptor; }
        return nullptr;
    };
    Require(cse::modules::Admit(lookup) != nullptr, "valid provider rejected");
    Require(cse::modules::AdmitConsumer(lookup), "valid GameUI consumer rejected");
    const auto original = factory;
    for (int field = 0; field < 10; ++field) {
        factory = original;
        switch (field) {
            case 0: factory.bytes = 0; break;
            case 1: ++factory.revision; break;
            case 2: ++factory.descriptor.architecture; break;
            case 3: factory.descriptor.pointerBits = 32; break;
            case 4: ++factory.descriptor.compilerAbi; break;
            case 5: ++factory.descriptor.tables[0].signature; break;
            case 6: factory.Create = nullptr; break;
            case 7: factory.Release = nullptr; break;
            case 8: factory.descriptor.tableCount = 0; break;
            case 9: --factory.descriptor.descriptorBytes; break;
        }
        Require(!cse::modules::Admit(lookup), "invalid provider admitted");
        if (field >= 2 && field <= 5) {
            Require(!cse::modules::AdmitConsumer(lookup), "invalid GameUI consumer admitted");
        }
    }
    factory = original;
    auto missing = +[](const char*, int*) -> void* { return nullptr; };
    Require(!cse::modules::Admit(missing), "legacy provider admitted");
    Require(!cse::modules::AdmitConsumer(missing), "old GameUI admitted");
}

static void (*originalProviderRelease)(ISystemModule *) = nullptr;
static unsigned int providerReleaseCount = 0;
static void CountProviderRelease(ISystemModule *module) {
    ++providerReleaseCount;
    originalProviderRelease(module);
}

class SelfRemovingInit : public BaseSystemModule {
public:
    int initCalls = 0;
    bool reentrantAccepted = false;
    bool Init(IBaseSystem *system, int serial, char *name) override {
        BaseSystemModule::Init(system, serial, name);
        ++initCalls;
        if (initCalls == 1) {
            system->RemoveModule(this);
            reentrantAccepted = system->AddModule(this, name);
        }
        return true;
    }
};

class RemovalChild : public BaseSystemModule {
public:
    ISystemModule *parent = nullptr;
    bool reentrantAccepted = false;
    void ShutDown() override {
        reentrantAccepted = m_System->AddModule(parent, const_cast<char *>("parent-reentry"));
        BaseSystemModule::ShutDown();
    }
};

class RemovalParent : public BaseSystemModule {
public:
    RemovalChild child;
    bool Init(IBaseSystem *system, int serial, char *name) override {
        BaseSystemModule::Init(system, serial, name);
        child.parent = this;
        return system->AddModule(&child, const_cast<char *>("child"));
    }
};

class OrderedModule : public BaseSystemModule {
public:
    std::vector<int> *order = nullptr;
    int id = 0;
    OrderedModule *child = nullptr;
    bool Init(IBaseSystem *system, int serial, char *name) override {
        BaseSystemModule::Init(system, serial, name);
        return !child || system->AddModule(child, const_cast<char *>("ordered-child"));
    }
    void ShutDown() override {
        if (m_State != MODULE_DISCONNECTED) {
            order->push_back(id);
        }
        BaseSystemModule::ShutDown();
    }
};

static void RegistryRegression(SystemWrapper &system, const std::string &mode) {
    if (mode == "publication") {
        std::vector<int> order;
        OrderedModule oldest;
        OrderedModule parent;
        OrderedModule child;
        oldest.order = parent.order = child.order = &order;
        oldest.id = 1;
        parent.id = 2;
        child.id = 3;
        parent.child = &child;
        for (int retry = 0; retry < 2; ++retry) {
            if (retry) {
                Require(system.Init(&system, 0, const_cast<char *>("order-retry")), "system retry failed");
            }
            Require(system.AddModule(&oldest, const_cast<char *>("oldest")), "oldest setup failed");
            Require(system.AddModule(&parent, const_cast<char *>("parent")), "parent setup failed");
            if (retry) {
                system.RemoveModule(&oldest);
                Require(system.AddModule(&oldest, const_cast<char *>("reactivated")), "oldest retry failed");
            }
            order.clear();
            system.ShutDown();
            const std::vector<int> expected = retry ? std::vector<int>{1, 2, 3} : std::vector<int>{2, 3, 1};
            Require(order == expected, "shutdown did not follow reverse publication order");
        }
    } else if (mode == "reactivation") {
        auto *server = cse::modules::Acquire<IServer>(&system, cse::modules::ServerName, "core", "owned-server");
        Require(server, "owned server missing");
        auto *provider = const_cast<cse::modules::Factory *>(
            cse::modules::Admit(system.GetLibrary(const_cast<char *>("core"))->createInterfaceFn));
        originalProviderRelease = provider->Release;
        provider->Release = CountProviderRelease;
        providerReleaseCount = 0;
        system.RemoveModule(server);
        auto incompatibleProvider = *provider;
        Require(!system.m_NativeModules.Add(&system, server, nullptr, &incompatibleProvider,
            [](ISystemModule *) {}), "incompatible ownership change accepted");
        Require(system.AddModule(server, const_cast<char *>("reactivated-server")), "reactivation rejected");
        system.ShutDown();
        Require(providerReleaseCount == 1, "reactivated provider object lost its release obligation");
    } else if (mode == "removal-frame") {
        RemovalParent module;
        Require(system.AddModule(&module, const_cast<char *>("parent")), "parent setup failed");
        system.RemoveModule(&module);
        Require(!module.child.reentrantAccepted, "same-pointer reactivation during removal accepted");
        Require(!system.m_NativeModules.Find(&module), "parent revived during child cleanup");
        Require(system.AddModule(&module, const_cast<char *>("parent-retry")), "parent retry failed");
        system.RemoveModule(&module);
        system.ShutDown();
    } else {
        SelfRemovingInit module;
        const bool result = system.AddModule(&module, const_cast<char *>("self-removing"));
        Require(!result, "self-removing initialization became published");
        Require(!module.reentrantAccepted && module.initCalls == 1, "in-flight same-pointer reactivation accepted");
        Require(!system.m_NativeModules.Find(&module), "self-removing module remained active");
        Require(system.AddModule(&module, const_cast<char *>("retry")), "retry after Init unwound failed");
        system.RemoveModule(&module);
        system.ShutDown();
    }
}

class NameCopyModule : public BaseSystemModule {
public:
    static constexpr std::size_t NameCapacity = sizeof(m_Name);

    void PoisonName() {
        std::fill(m_Name, m_Name + sizeof(m_Name), '!');
    }
};

static void NameCopyPreservation(SystemWrapper &system) {
    NameCopyModule module;
    const auto check = [&](char *input, const char *expected, std::size_t expectedLength) {
        module.PoisonName();
        Require(module.Init(&system, 1, input), "name-copy Init failed");
        const char *actual = module.GetName();
        Require(std::memcmp(actual, expected, expectedLength) == 0, "module name bytes changed");
        Require(std::all_of(actual + expectedLength, actual + NameCopyModule::NameCapacity,
                           [](char value) {
                               return value == '\0';
                           }),
                "module name terminator or zero padding changed");
    };

    char empty[] = "";
    char shortName[] = "module";
    char embeddedNull[] = {'a', '\0', 'b', '\0'};
    check(nullptr, "", 0);
    check(empty, "", 0);
    check(shortName, "module", sizeof(shortName) - 1);
    check(embeddedNull, "a", 1);

    constexpr std::size_t MaxNameLength = NameCopyModule::NameCapacity - 1;
    std::array<char, NameCopyModule::NameCapacity> boundaryName;
    boundaryName.fill('b');
    boundaryName.back() = '\0';
    check(boundaryName.data(), boundaryName.data(), MaxNameLength);

    std::array<char, NameCopyModule::NameCapacity + 1> longName;
    longName.fill('l');
    longName.back() = '\0';
    check(longName.data(), longName.data(), MaxNameLength);

    // No terminator is accessible: Init must read at most MaxNameLength bytes.
    std::array<char, MaxNameLength> boundedName;
    boundedName.fill('n');
    check(boundedName.data(), boundedName.data(), boundedName.size());
    module.ShutDown();
    system.ShutDown();
}

int main(int argc, char** argv) {
    try {
        Require(argc >= 3, "provider paths required");
        if (argc == 4 && std::strncmp(argv[3], "cycle", 5) != 0) {
            SystemWrapper system;
            currentModuleSystem = &system;
            system.corePath = argv[1]; system.demoPath = argv[2];
            Require(system.Init(&system, 0, const_cast<char*>("failure")), "system init failed");
            const std::string mode = argv[3];
            if (mode == "name-copy") {
                NameCopyPreservation(system);
                return 0;
            }
            if (mode == "reactivation" || mode == "self-removal" || mode == "removal-frame" ||
                mode == "publication") {
                RegistryRegression(system, mode);
                return 0;
            }
#if defined(LISTENER_ALLOCATOR_INTERPOSITION)
            if (mode == "listener-allocation") {
                ListenerAllocationFailure(system);
                // Preload images and initialize the real Server before arming, so only
                // its first BorrowedListeners insertion is eligible for this null realloc.
                Require(system.GetLibrary(const_cast<char *>("demoplayer")), "demo image preload failed");
                auto *server = cse::modules::Acquire<IServer>(&system, cse::modules::ServerName, "core", "DemoServer");
                Require(server, "allocation fixture server missing");
                constexpr std::size_t InitialListenerCapacity = 8;
                ListenerAllocatorArm(nullptr, InitialListenerCapacity * sizeof(ISystemModule *));
            }
#endif
            if (mode == "server") { system.failFileSystem = true; }
            if (mode == "world") { system.failWorldInit = true; }
            if (mode == "command") { failCommandAllocation = true; }
            if (mode == "subscription") {
                auto* server = cse::modules::Acquire<IServer>(&system, cse::modules::ServerName, "core", "DemoServer");
                Require(server, "subscription fixture server missing");
                auto* api = cse::modules::Query<cse::modules::Subscription>(server, cse::modules::SubscriptionName);
                Require(api, "subscription API missing");
                api->Register = [](void*, ISystemModule*) { return false; };
            }
            if (mode == "engine") { system.RemoveModule(system.m_EngineWrapper); }
            auto* demo = cse::modules::Acquire<IDemoPlayer>(&system, cse::modules::DemoName, "demoplayer");
            Require(!demo, "failed DemoPlayer Init returned success");
#if defined(LISTENER_ALLOCATOR_INTERPOSITION)
            if (mode == "listener-allocation") {
                Require(ListenerAllocatorFailures() == 1, "DemoPlayer subscription did not reach realloc");
                constexpr int MaxAllocationFrames = 24;
                void *frames[MaxAllocationFrames]{};
                const int frameCount = ListenerAllocatorFrames(frames, MaxAllocationFrames);
                bool realDemoFrame = false;
                for (int index = 0; index < frameCount; ++index) {
                    Dl_info frame{};
                    if (dladdr(frames[index], &frame) && frame.dli_fname) {
                        realDemoFrame |= std::strstr(frame.dli_fname, "libdemoplayer.dylib") != nullptr;
                        std::printf("allocation frame: %s %s\n", frame.dli_fname,
                            frame.dli_sname ? frame.dli_sname : "<inlined>");
                    }
                }
                Require(realDemoFrame, "allocation failure was not called through real DemoPlayer");
                Dl_info caller{};
                Require(dladdr(ListenerAllocatorCaller(), &caller) && caller.dli_fname,
                    "allocation call site was not identifiable");
                std::printf("DemoPlayer listener realloc failure: image=%s symbol=%s\n", caller.dli_fname,
                    caller.dli_sname ? caller.dli_sname : "<inlined>");
            }
#endif
            Require(!system.FindModule(const_cast<char*>(cse::modules::ServerName), nullptr), "stale server after failure");
            Require(!system.FindModule(const_cast<char*>(cse::modules::WorldName), nullptr), "stale world after failure");
            Require(!engineCommands.count("dem_pause"), "stale command after failure");
            system.failFileSystem = false; system.failWorldInit = false; failCommandAllocation = false;
            if (mode == "engine") { Require(system.AddModule(system.m_EngineWrapper, nullptr), "engine retry failed"); }
            demo = cse::modules::Acquire<IDemoPlayer>(&system, cse::modules::DemoName, "demoplayer");
            Require(demo, "reinitialization after failure failed");
            auto *world = demo->GetWorld();
            Require(world, "retry world missing");
            world->SetPaused(true);
            world->SetPaused(false);
            system.RemoveModule(demo);
            Require(!system.FindModule(const_cast<char *>(cse::modules::DemoName), nullptr), "retry demo remained");
            Require(!system.FindModule(const_cast<char *>(cse::modules::ServerName), nullptr), "retry server remained");
            Require(!system.FindModule(const_cast<char *>(cse::modules::WorldName), nullptr), "retry world remained");
            Require(!engineCommands.count("dem_pause"), "retry command remained");
            system.ShutDown();
            return 0;
        }
        Admission();
        const int firstCycle = argc == 4 ? std::atoi(argv[3] + 5) : 0;
        const int lastCycle = argc == 4 ? firstCycle + 1 : 4;
        for (int cycle = firstCycle; cycle < lastCycle; ++cycle) {
            SystemWrapper system;
            currentModuleSystem = &system;
            system.corePath = argv[1]; system.demoPath = argv[2];
            Require(system.Init(&system, 0, const_cast<char*>("fixture")), "system init failed");
            auto* engine = cse::modules::Acquire<IEngineWrapper>(&system, cse::modules::EngineName, "");
            Require(engine && engine->GetTraceEntity() == 42, "native engine registration failed");
            auto* demo = cse::modules::Acquire<IDemoPlayer>(&system, cse::modules::DemoName, "demoplayer");
            Require(demo, "nested DemoPlayer acquisition failed");
            auto* server = cse::modules::Acquire<IServer>(&system, cse::modules::ServerName, "", "DemoServer");
            auto* world = demo->GetWorld();
            Require(server && world, "nested children missing");
            const std::set<int> serials{engine->GetSerial(), demo->GetSerial(), server->GetSerial(), world->GetSerial()};
            Require(serials.size() == 4, "duplicate nested serials");
            auto* director = cse::modules::Query<IDirector>(demo, cse::modules::DirectorName);
            Require(director && static_cast<ISystemModule*>(director) == static_cast<ISystemModule*>(demo),
                "secondary director has wrong canonical identity");
            Require(director->GetCommands() != nullptr, "late director dispatch failed");
            const auto* core = cse::modules::Admit(system.GetLibrary(const_cast<char*>("core"))->createInterfaceFn);
            ISystemModule* networkBase = core->Create(cse::modules::NetworkName);
            auto* network = cse::modules::Query<INetwork>(networkBase, cse::modules::NetworkName);
            Require(network && static_cast<ISystemModule*>(network) == networkBase, "network secondary view mismatch");
            Require(network->GetErrorText(0), "late network query failed");
            // Frozen legacy GetModule conversion cannot adjust this real secondary interface pointer.
            Require(static_cast<void*>(network) != static_cast<void*>(networkBase), "crosscast negative control lost");
            core->Release(networkBase);
            Require(!core->Create("unsupported"), "unsupported provider creation accepted");
            void* legacyView = system.GetLibrary(const_cast<char*>("core"))->createInterfaceFn(
                cse::modules::NetworkName, nullptr);
            auto* legacyNetwork = static_cast<INetwork*>(legacyView);
            auto* adjustedLegacyBase = static_cast<ISystemModule*>(legacyNetwork);
            Require(legacyView != static_cast<void*>(adjustedLegacyBase), "legacy factory mismatch not reproduced");
            core->Release(adjustedLegacyBase);
            Require(!demo->QueryInterface("unknown"), "unknown query accepted");
            demo->SetEditMode(true); Require(demo->IsEditMode(), "late demo dispatch failed");
            demo->SetTimeScale(0.5f); Require(demo->GetTimeScale() == 0.5f, "time scale call failed");
            std::array<unsigned char, 8> message{};
            Require(demo->ReadDemoMessage(message.data(), message.size()) == 0, "inactive message changed");
            server->SetRate(12000); Require(server->GetRate() == 12000, "late server call failed");
            world->SetPaused(true); Require(world->IsPaused(), "world signal/call failed");
            system.RunFrame(0.01);
            system.DispatchCommand(const_cast<char*>("dem_speed 2"));
            Require(demo->GetTimeScale() == 0.5f && system.printed.find("Not viewing a demo.") != std::string::npos,
                "production command dispatch did not preserve inactive-demo gate");
#if defined(CONSOLE_LOADER_FIXTURE)
            Require(system.m_Commands.CountElements() == 6, "console commands not registered");
#else
            Require(engineCommands.count("dem_pause") == 1, "commands not registered");
#endif
            Listener borrowed;
            Require(system.AddModule(&borrowed, const_cast<char*>("borrowed")), "borrowed registration failed");
            Require(system.FindModule(const_cast<char*>("GenericModule"), const_cast<char*>("borrowed")) == &borrowed,
                "generic local module lookup regressed");
            Listener publisher; Listener first; Listener second;
            publisher.Init(&system, 100, const_cast<char*>("publisher"));
            first.Init(&system, 4001, const_cast<char*>("first"));
            second.Init(&system, 4001, const_cast<char*>("second"));
            publisher.RegisterListener(&first); publisher.RegisterListener(&first); publisher.RegisterListener(&second);
            publisher.Emit(); Require(first.calls == 1 && second.calls == 1, "listener pointer identity/dedup failed");
            first.publisher = &publisher; first.remove = &second; first.nested = true;
            publisher.Emit(); Require(first.calls == 3 && second.calls == 1, "nested dispatch called removed listener");
            first.remove = &first; publisher.Emit(); publisher.Emit(); Require(first.calls == 4, "self-removal failed");
            if (cycle == 1) { system.RemoveModule(world); system.RemoveModule(server); }
            if (cycle == 2) { demo->ShutDown(); }
            system.RemoveModule(demo); system.RemoveModule(demo);
            Require(!system.FindModule(const_cast<char*>(cse::modules::DemoName), nullptr), "removed demo still published");
            Require(!engineCommands.count("dem_pause"), "removed engine command remains");
            auto* restarted = cse::modules::Acquire<IDemoPlayer>(&system, cse::modules::DemoName, "demoplayer");
            Require(restarted, "same-registry singleton reinitialization failed");
            if (cycle == 3) {
                Listener shutdownListener;
                shutdownListener.Init(&system, 4001, const_cast<char*>("shutdown"));
                shutdownListener.shutdown = true;
                auto* publisher = restarted->GetWorld();
                publisher->RegisterListener(&shutdownListener);
                publisher->SetPaused(true);
                Require(system.m_Libraries.CountElements() > 0, "provider unloaded inside callback");
                Require(system.m_NativeModules.lifetime.depth == 0, "callback scope leaked");
                system.RunFrame(0);
            } else {
                system.RemoveModule(restarted);
            }
            system.ShutDown(); system.ShutDown();
            Require(system.m_Modules.CountElements() == 0, "modules remain after shutdown");
            Require(std::strcmp(borrowed.GetName(), "borrowed") == 0, "borrowed allocation was destroyed");
        }
        if (argc == 3) {
            currentModuleSystem = &gSystemWrapper;
            gSystemWrapper.corePath = argv[1]; gSystemWrapper.demoPath = argv[2];
            Require(gSystemWrapper.Init(&gSystemWrapper, 0, nullptr), "cache fixture init failed");
            Require(DemoPlayer_Init() == 1, "engine cache acquisition failed");
            DemoPlayer_SetActive(1);
            Require(DemoPlayer_IsActive(), "engine cache inactive");
            gSystemWrapper.ShutDown();
            Require(!DemoPlayer_IsActive(), "engine cache survives unload");
        }
        std::puts("real loader + real DemoPlayer/Core lifecycle passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
