#include "public/FileSystem.h"
#include "actual_declarations.h"
#include "public/cse_engine_bootstrap_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bootstrap = cse::engine_bootstrap_abi;

constexpr auto Engine = bootstrap::DescribeEngine<EngineProducer::IEngineAPI,
    EngineProducer::IDedicatedServerAPI, EngineProducer::IDedicatedExports, CreateInterfaceFn>();
constexpr auto Launcher = bootstrap::DescribeLauncher<LauncherCaller::IEngineAPI, CreateInterfaceFn>();
constexpr auto Dedicated = bootstrap::DescribeDedicatedHost<DedicatedCaller::IDedicatedServerAPI,
    DedicatedExportsCaller::IDedicatedExports, CreateInterfaceFn>();

static_assert(cse::abi::RuntimeVersion == 10, "Foundation uses the coordinated runtime epoch");
static_assert(sizeof(cse::abi::Descriptor) == 288, "Foundation preserves the descriptor layout");
static_assert(cse::abi::CheckRequired(bootstrap::RequireRole(Launcher, bootstrap::Role::EngineProvider), &Engine)
    == cse::abi::Compatibility::Compatible, "Real owner/caller Run shapes and mapped roles agree");
static_assert(cse::abi::CheckRequired(bootstrap::RequireRole(Dedicated, bootstrap::Role::EngineProvider), &Engine)
    == cse::abi::Compatibility::Compatible, "Real dedicated owner/caller declarations agree");
static_assert(cse::abi::CheckRequired(bootstrap::RequireRole(Engine, bootstrap::Role::DedicatedHost), &Dedicated)
    == cse::abi::Compatibility::Compatible, "Dedicated callback direction is explicitly checked");

static_assert(!bootstrap::EngineDeclaration<EngineProducer::IEngineAPI, CreateInterfaceFn>::Launcher,
    "Mutable producer spelling is not silently accepted as the const-spelled caller");
static_assert(!bootstrap::EngineDeclaration<LauncherCaller::IEngineAPI, CreateInterfaceFn>::Producer,
    "Const-spelled caller is not silently accepted as the mutable producer");
static_assert(sizeof(EngineProducer::IEngineAPI) == sizeof(LauncherCaller::IEngineAPI),
    "The source-role negative controls have identical object sizes");

namespace {
int checks = 0;
int factoryCalls = 0;
int hostCalls = 0;
int filesystemCalls = 0;
int printCalls = 0;
int consoleCalls = 0;
int filesystemMarker = 0;
constexpr float FixtureFramesPerSecond = 13.5f;
constexpr int FixtureActivePlayers = 2;
constexpr int FixtureMaximumPlayers = 32;

void Check(bool condition) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL check %d\n", checks);
        std::abort();
    }
}

class WrongSignedShutdown : public IBaseInterface {
public:
    virtual bool Init(char*, char*, CreateInterfaceFn, CreateInterfaceFn) = 0;
    virtual unsigned int Shutdown() = 0;
    virtual bool RunFrame() = 0;
    virtual void AddConsoleText(char*) = 0;
    virtual void UpdateStatus(float*, int*, int*, char*) = 0;
};

class WrongStatusPointee : public IBaseInterface {
public:
    virtual bool Init(char*, char*, CreateInterfaceFn, CreateInterfaceFn) = 0;
    virtual int Shutdown() = 0;
    virtual bool RunFrame() = 0;
    virtual void AddConsoleText(char*) = 0;
    virtual void UpdateStatus(double*, int*, int*, char*) = 0;
};

class WrongPrintConst : public IBaseInterface {
public:
    virtual void Sys_Printf(const char*) = 0;
};

using WrongFactory = char* (CSE_ABI_CALL *)(const char*, int*);
static_assert(sizeof(WrongSignedShutdown) == sizeof(DedicatedCaller::IDedicatedServerAPI),
    "Changed signedness preserves the interface object size");
static_assert(sizeof(WrongStatusPointee) == sizeof(DedicatedCaller::IDedicatedServerAPI),
    "Changed output pointee preserves the interface object size");
static_assert(sizeof(WrongPrintConst) == sizeof(DedicatedExportsCaller::IDedicatedExports),
    "Changed source permission preserves the interface object size");
static_assert(sizeof(WrongFactory) == sizeof(CreateInterfaceFn), "Changed factory return preserves pointer size");
static_assert(!bootstrap::DedicatedDeclaration<WrongSignedShutdown, CreateInterfaceFn>::Valid,
    "Signedness is checked independently of size");
static_assert(!bootstrap::DedicatedDeclaration<WrongStatusPointee, CreateInterfaceFn>::Valid,
    "The float output record is checked beyond pointer width");
static_assert(!bootstrap::ExportsDeclaration<WrongPrintConst>::Valid,
    "Only the measured Run command pointer spelling is normalized");
static_assert(!bootstrap::EngineDeclaration<EngineProducer::IEngineAPI, WrongFactory>::Producer,
    "A factory's nested return role cannot be replaced by another same-sized pointer type");

class HostExports final : public EngineProducer::IDedicatedExports {
public:
    void Sys_Printf(char* text) override {
        ++printCalls;
        Check(std::strcmp(text, "fixture callback") == 0);
    }
};

HostExports hostExports;

void* CSE_ABI_CALL HostFactory(const char* name, int* status) {
    ++hostCalls;
    Check(std::strcmp(name, "VENGINE_DEDICATEDEXPORTS_API_VERSION001") == 0);
    if (status) {
        *status = IFACE_OK;
    }
    return static_cast<EngineProducer::IDedicatedExports*>(&hostExports);
}

void* CSE_ABI_CALL FilesystemFactory(const char* name, int* status) {
    ++filesystemCalls;
    Check(std::strcmp(name, "VFileSystem009") == 0);
    if (status) {
        *status = IFACE_OK;
    }
    // This is an ordering marker, never cast to or used as an IFileSystem.
    // Nested filesystem dispatch is covered by its real catalog and compiler
    // call probes. This fixture does not execute a filesystem provider.
    return &filesystemMarker;
}

class EngineObject final : public EngineProducer::IEngineAPI {
public:
    int Run(void* instance, char* base, char* command, char* restart, CreateInterfaceFn launcher,
        CreateInterfaceFn filesystem) override {
        Check(instance == nullptr);
        Check(std::strcmp(base, "owned base") == 0);
        Check(std::strcmp(command, "owned command") == 0);
        Check(launcher == HostFactory);
        Check(filesystem == FilesystemFactory);
        Check(filesystem("VFileSystem009", nullptr) == &filesystemMarker);
        constexpr char RestartText[] = " +fixture";
        std::memcpy(restart, RestartText, sizeof(RestartText));
        return static_cast<int>(bootstrap::RunResult::None);
    }
};

class DedicatedObject final : public EngineProducer::IDedicatedServerAPI {
public:
    bool Init(char* base, char* command, CreateInterfaceFn launcher, CreateInterfaceFn filesystem) override {
        Check(std::strcmp(base, "owned base") == 0);
        Check(std::strcmp(command, "owned command") == 0);
        auto* exports = static_cast<EngineProducer::IDedicatedExports*>(
            launcher("VENGINE_DEDICATEDEXPORTS_API_VERSION001", nullptr));
        char text[] = "fixture callback";
        exports->Sys_Printf(text);
        initialized = filesystem("VFileSystem009", nullptr) == &filesystemMarker;
        return initialized;
    }

    int Shutdown() override {
        initialized = false;
        return 0;
    }

    bool RunFrame() override {
        return initialized;
    }

    void AddConsoleText(char* text) override {
        Check(std::strcmp(text, "owned console") == 0);
        ++consoleCalls;
    }

    void UpdateStatus(float* fps, int* active, int* maximum, char* map) override {
        *fps = FixtureFramesPerSecond;
        *active = FixtureActivePlayers;
        *maximum = FixtureMaximumPlayers;
        constexpr char MapName[] = "fixture";
        std::memcpy(map, MapName, sizeof(MapName));
    }

private:
    bool initialized = false;
};

EngineObject engineObject;
DedicatedObject dedicatedObject;

void* CSE_ABI_CALL EngineFactory(const char* name, int* status) {
    ++factoryCalls;
    if (status) {
        *status = IFACE_OK;
    }
    if (std::strcmp(name, "VENGINE_LAUNCHER_API_VERSION002") == 0) {
        return static_cast<EngineProducer::IEngineAPI*>(&engineObject);
    }
    Check(std::strcmp(name, "VENGINE_HLDS_API_VERSION002") == 0);
    return static_cast<EngineProducer::IDedicatedServerAPI*>(&dedicatedObject);
}

bool Gate(const cse::abi::Descriptor* provider, const cse::abi::Descriptor* host, bootstrap::Role hostRole,
    const cse::abi::Descriptor* filesystem, const char* interfaceName, void** result) {
    *result = nullptr;
    if (cse::abi::CheckRequired(bootstrap::RequireRole(Launcher, bootstrap::Role::EngineProvider), provider)
        != cse::abi::Compatibility::Compatible) {
        return false;
    }
    if (cse::abi::CheckRequired(bootstrap::RequireRole(Engine, hostRole), host)
        != cse::abi::Compatibility::Compatible) {
        return false;
    }
    if (cse::abi::CheckRequired(cse::filesystem_abi::Descriptor, filesystem)
        != cse::abi::Compatibility::Compatible) {
        return false;
    }
    *result = CreateInterface(interfaceName, nullptr);
    return *result != nullptr;
}

void Reject(const cse::abi::Descriptor* provider, const cse::abi::Descriptor* host,
    const cse::abi::Descriptor* filesystem) {
    const int beforeFactory = factoryCalls;
    const int beforeHost = hostCalls;
    const int beforeFilesystem = filesystemCalls;
    void* result = &filesystemMarker;
    Check(!Gate(provider, host, bootstrap::Role::Launcher, filesystem,
        "VENGINE_LAUNCHER_API_VERSION002", &result));
    Check(result == nullptr);
    Check(factoryCalls == beforeFactory);
    Check(hostCalls == beforeHost);
    Check(filesystemCalls == beforeFilesystem);
}
}

extern "C" void* CSE_ABI_CALL CreateInterface(const char* name, int* status) {
    return EngineFactory(name, status);
}

static_assert(std::is_same<decltype(&CreateInterface), CreateInterfaceFn>::value,
    "The actual factory declaration preserves its typed call convention");

int main() {
    Reject(nullptr, &Launcher, &cse::filesystem_abi::Descriptor);
    Reject(&Engine, nullptr, &cse::filesystem_abi::Descriptor);
    Reject(&Engine, &Launcher, nullptr);
    Reject(&Launcher, &Launcher, &cse::filesystem_abi::Descriptor);
    Reject(&Engine, &Dedicated, &cse::filesystem_abi::Descriptor);
    for (std::size_t index = 0; index < Engine.tableCount; ++index) {
        auto changed = Engine;
        changed.tables[index].signature ^= 1;
        Reject(&changed, &Launcher, &cse::filesystem_abi::Descriptor);
        changed = Engine;
        changed.tables[index].identifier ^= 1;
        Reject(&changed, &Launcher, &cse::filesystem_abi::Descriptor);
    }
    constexpr int EnvelopeFieldsUnderTest = 5;
    for (int field = 0; field < EnvelopeFieldsUnderTest; ++field) {
        auto changed = Engine;
        if (field == 0) {
            changed.descriptorVersion ^= 1;
        } else if (field == 1) {
            changed.runtimeVersion ^= 1;
        } else if (field == 2) {
            changed.architecture ^= 1;
        } else if (field == 3) {
            changed.pointerBits ^= 1;
        } else {
            changed.compilerAbi ^= 1;
        }
        Reject(&changed, &Launcher, &cse::filesystem_abi::Descriptor);
    }
    auto duplicate = Engine;
    duplicate.tables[duplicate.tableCount++] = duplicate.tables[0];
    Reject(&duplicate, &Launcher, &cse::filesystem_abi::Descriptor);
    auto invalidCount = Engine;
    invalidCount.tableCount = cse::abi::MaxTables + 1;
    Reject(&invalidCount, &Launcher, &cse::filesystem_abi::Descriptor);
    auto wrongFilesystem = cse::filesystem_abi::Descriptor;
    wrongFilesystem.tables[1].signature ^= 1;
    Reject(&Engine, &Launcher, &wrongFilesystem);

    auto extended = Engine;
    constexpr std::uint32_t UnrelatedTestFamily = 0x7fff1000;
    extended.tables[extended.tableCount++] = {UnrelatedTestFamily, 0, cse::abi::LayoutHash({1})};
    void* view = nullptr;
    Check(Gate(&extended, &Launcher, bootstrap::Role::Launcher, &cse::filesystem_abi::Descriptor,
        "VENGINE_LAUNCHER_API_VERSION002", &view));
    char base[] = "owned base";
    char command[] = "owned command";
    char restart[bootstrap::RestartBufferBytes] = {};
    Check(static_cast<EngineProducer::IEngineAPI*>(view)->Run(nullptr, base, command, restart,
        HostFactory, FilesystemFactory) == static_cast<int>(bootstrap::RunResult::None));
    Check(std::strcmp(restart, " +fixture") == 0);
    Check(hostCalls == 0);
    Check(Gate(&Engine, &Dedicated, bootstrap::Role::DedicatedHost, &cse::filesystem_abi::Descriptor,
        "VENGINE_HLDS_API_VERSION002", &view));
    auto* dedicatedApi = static_cast<EngineProducer::IDedicatedServerAPI*>(view);
    Check(dedicatedApi->Init(base, command, HostFactory, FilesystemFactory));
    Check(dedicatedApi->RunFrame());
    char console[] = "owned console";
    dedicatedApi->AddConsoleText(console);
    float fps = 0.0f;
    int active = 0;
    int maximum = 0;
    char map[bootstrap::StatusMapBytes] = {};
    dedicatedApi->UpdateStatus(&fps, &active, &maximum, map);
    Check(fps == FixtureFramesPerSecond);
    Check(active == FixtureActivePlayers);
    Check(maximum == FixtureMaximumPlayers);
    Check(std::strcmp(map, "fixture") == 0);
    Check(dedicatedApi->Shutdown() == 0);
    Check(!dedicatedApi->RunFrame());
    Check(std::strcmp(command, "owned command") == 0);
    Check(std::strcmp(console, "owned console") == 0);
    Check(factoryCalls == 2);
    Check(hostCalls == 1);
    Check(filesystemCalls == 2);
    Check(printCalls == 1);
    Check(consoleCalls == 1);
    std::printf("PASS %d checks; modeled factories=%d host=%d fs=%d; compiler profile=%llu\n", checks,
        factoryCalls, hostCalls, filesystemCalls, static_cast<unsigned long long>(cse::filesystem_abi::Profile));
    return 0;
}
