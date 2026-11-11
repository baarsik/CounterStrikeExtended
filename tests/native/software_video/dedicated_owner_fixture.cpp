// Included after the verbatim production dedicated class and method definitions.
CDedicatedServerAPI::~CDedicatedServerAPI() = default;
void CDedicatedServerAPI::AddConsoleText(char *) {}
void CDedicatedServerAPI::UpdateStatus(float *, int *, int *, char *) {}

static CDedicatedServerAPI *owner;
static std::vector<std::string> cleanup;
static bool probing;
static bool failGameShutdown;
static bool failEngineUnload;
static int transportCloses;
static int factoryCalls;
static char baseDirectory[] = ".";
static char commandLine[] = "-dedicated -nobreakpad";
class OwnerExports final : public IDedicatedExports {
public:
    void Sys_Printf(char*) override {}
};
static OwnerExports exportsStorage;
static IDedicatedExports* exportsObject = &exportsStorage;

extern "C" __attribute__((visibility("default"))) void *CreateInterface(const char *, int *) {
    ++factoryCalls;
    return exportsObject;
}

static void ProbeReentrancy(const char *boundary) {
    if (probing) {
        return;
    }
    probing = true;
    const auto previousCleanup = cleanup;
    const int previousFactories = factoryCalls;
    owner->Shutdown();
    if (cleanup != previousCleanup) {
        std::fprintf(stderr, "dependent cleanup advanced during %s\n", boundary);
    }
    assert(cleanup == previousCleanup);
    assert(!owner->Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
    assert(factoryCalls == previousFactories);
    assert(owner->Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
    assert(transportCloses == 0);
    probing = false;
}

void Engine::Unload() {
    host_initialized = false;
    ProbeReentrancy("engine unload");
    if (failEngineUnload) {
        throw std::runtime_error("controlled engine cleanup failure");
    }
    cleanup.emplace_back("engine");
}

bool Game::Shutdown() {
    ProbeReentrancy("game shutdown");
    if (failGameShutdown) {
        return false;
    }
    cleanup.emplace_back("game");
    return true;
}

void FileSystem_Shutdown() {
    ProbeReentrancy("filesystem shutdown");
    cleanup.emplace_back("filesystem");
}

void Registry::Shutdown() {
    ProbeReentrancy("registry shutdown");
    cleanup.emplace_back("registry");
}

void Sys_ShutdownArgv() {
    ProbeReentrancy("argument shutdown");
    cleanup.emplace_back("arguments");
}

bool CseSteamRetireServerTransport() {
    ++transportCloses;
    return true;
}

#include <mach-o/dyld.h>
namespace cse::software_lifetime {
bool RetireResources(Mode) {
    return false; // This fixture exercises session owners; the separate terminal suite owns final retirement.
}
}
extern "C" const cse::software_lifetime::Api CseNativeSoftwareLifetimeV1;
static bool NoTerminalAdmission(void*, const void*, std::uint64_t) noexcept {
    return false;
}
static void FixtureNetworkLog(ESteamNetworkingSocketsDebugOutputType, const char*) {}

int main() {
    using namespace cse::software_lifetime;
    const Binding binding{sizeof(Binding), Version, 1, _dyld_get_image_header(0), &owner,
        NoTerminalAdmission, FixtureNetworkLog, Mode::Dedicated, 0};
    assert(CseNativeSoftwareLifetimeV1.bind(&binding) == Result::Ready);
    {
        CDedicatedServerAPI candidate;
        owner = &candidate;
        assert(candidate.Retire(0) == CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION);
        assert(candidate.Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SHUTDOWN_REQUIRED);
        const int callsBefore = factoryCalls;
        std::string oversized(1024, 'a');
        assert(!candidate.Init(baseDirectory, oversized.data(), CreateInterface, CreateInterface));
        assert(factoryCalls == callsBefore && cleanup.empty());
        std::string excessiveArguments;
        for (int index = 0; index < MAX_NUM_ARGVS; ++index) {
            excessiveArguments += "a ";
        }
        assert(!candidate.Init(baseDirectory, excessiveArguments.data(), CreateInterface, CreateInterface));
        for (std::string invalid : {"unterminated \"value", "embedded\"quote", "bad\x01value"}) {
            assert(!candidate.Init(baseDirectory, invalid.data(), CreateInterface, CreateInterface));
        }
        assert(factoryCalls == callsBefore && cleanup.empty());
        std::string quoted = "hlds -console +hostname \"Zażółć gęślą\" -game \"my mod\"";
        assert(candidate.Init(baseDirectory, quoted.data(), CreateInterface, CreateInterface));
        assert(com_argc == 7);
        assert(std::string(com_argv[4]) == "Zażółć gęślą");
        assert(std::string(com_argv[6]) == "my mod");
        candidate.Shutdown();
        cleanup.clear();
        std::string exact(1023, 'a');
        assert(candidate.Init(baseDirectory, exact.data(), CreateInterface, CreateInterface));
        candidate.Shutdown();
        cleanup.clear();
        failFilesystemInit = true;
        assert(!candidate.Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
        const std::vector<std::string> partial = {"filesystem", "registry", "arguments"};
        assert(cleanup == partial);
        candidate.Shutdown();
        assert(cleanup == partial);
        failFilesystemInit = false;
        cleanup.clear();
    }

    const std::vector<std::string> completeOrder = {"engine", "game", "filesystem", "registry", "arguments"};
    for (int scenario = 0; scenario < 3; ++scenario) {
        CDedicatedServerAPI candidate;
        owner = &candidate;
        cleanup.clear();
        transportCloses = 0;
        assert(candidate.Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
        // Initial no-op cleanup has no external owners and does not trigger probes.
        cleanup.clear();
        if (scenario == 1) {
            failGameShutdown = true;
            candidate.Shutdown();
            assert(cleanup == std::vector<std::string>{"engine"});
            assert(candidate.Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
            const int previousFactories = factoryCalls;
            assert(!candidate.Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
            assert(factoryCalls == previousFactories);
            failGameShutdown = false;
        } else if (scenario == 2) {
            failEngineUnload = true;
            bool caught = false;
            try {
                candidate.Shutdown();
            } catch (const std::runtime_error &) {
                caught = true;
            }
            assert(caught && cleanup.empty());
            assert(candidate.Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_SESSION_ACTIVE);
            failEngineUnload = false;
        }
        candidate.Shutdown();
        assert(cleanup == completeOrder);
        candidate.Shutdown();
        assert(cleanup == completeOrder);
        assert(candidate.Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
        cleanup.clear();
        candidate.Shutdown();
        assert(cleanup == completeOrder);
        assert(candidate.Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_READY);
        assert(candidate.Retire(CSE_NATIVE_DEDICATED_LIFETIME_VERSION) == CSE_DEDICATED_RETIRE_ALREADY_RETIRED);
        assert(transportCloses == 1);
        const int retiredFactoryCount = factoryCalls;
        assert(!candidate.Init(baseDirectory, commandLine, CreateInterface, CreateInterface));
        assert(factoryCalls == retiredFactoryCount);
    }
    std::puts("PASS dedicated owner nested Shutdown/Init/Retire, ordered cleanup, failed cleanup retry and terminal retirement");
}

extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetDedicatedAbiV1() {
    return &cse::dedicated_abi::Host;
}
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetModuleAbi() {
    return &cse::filesystem_abi::Descriptor;
}
