#include "check.h"
#include "common/IVGuiModule.h"
#include "native/ServerBrowserModule.h"
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#endif

namespace {
std::vector<std::string> events;
int loads = 0;
int unloads = 0;
int fakeHandleQueries = 0;
bool initializeResult = true;
bool postInitializeResult = true;
bool externalImage = false;
bool rejectFactory = false;
std::string recordName = "Servers";
std::string recordInterface = "VGuiModuleServerBrowser001";
std::function<void()> duringShutdown;
void Event(const char *name) {
    events.emplace_back(name);
}
} // namespace

class KeyValues {
public:
    explicit KeyValues(const char *name) : name_(name) {}
    KeyValues(const char *name, const char *field, const char *value) : name_(name) {
        values_[field] = value;
    }
    const char *GetName() const {
        return name_.c_str();
    }
    const char *GetString(const char *key, const char *fallback = "") const {
        const auto found = values_.find(key);
        return found == values_.end() ? fallback : found->second.c_str();
    }
    int GetInt(const char *key, int fallback = 0) const {
        const auto found = values_.find(key);
        return found == values_.end() ? fallback : std::atoi(found->second.c_str());
    }
    bool LoadFromFile(void *, const char *, const char *) {
        auto menu = std::make_unique<KeyValues>("Menu");
        auto servers = std::make_unique<KeyValues>(recordName.c_str());
        servers->values_["dll"] = "servers/serverbrowser.dll";
        servers->values_["interface"] = recordInterface;
        servers->values_["MenuName"] = "#App_Servers";
        menu->children_.push_back(std::move(servers));
        children_.push_back(std::move(menu));
        return true;
    }
    KeyValues *FindKey(const char *name, bool) {
        for (const auto &child : children_) {
            if (child->name_ == name) {
                return child.get();
            }
        }
        return nullptr;
    }
    KeyValues *GetFirstSubKey() {
        return children_.empty() ? nullptr : children_.front().get();
    }
    KeyValues *GetNextKey() {
        return nullptr;
    }
    KeyValues *MakeCopy() {
        return new KeyValues(name_.c_str());
    }
    void deleteThis() {
        delete this;
    }

private:
    std::string name_;
    std::map<std::string, std::string> values_;
    std::vector<std::unique_ptr<KeyValues>> children_;
};

template <class Type> class CUtlVector {
public:
    int Count() const {
        return static_cast<int>(values_.size());
    }
    int AddToTail() {
        values_.emplace_back();
        return Count() - 1;
    }
    bool IsValidIndex(int index) const {
        return index >= 0 && index < Count();
    }
    void RemoveAll() {
        values_.clear();
    }
    Type &operator[](int index) {
        return values_.at(static_cast<size_t>(index));
    }

private:
    std::vector<Type> values_;
};

class Module final : public IVGuiModule {
public:
    bool Initialize(CreateInterfaceFn *, int) override {
        Event("initialize");
        return initializeResult;
    }
    bool PostInitialize(CreateInterfaceFn *, int) override {
        Event("post-initialize");
        return postInitializeResult;
    }
    bool Activate() override {
        Event("activate");
        return true;
    }
    bool IsValid() override {
        return initializeResult && postInitializeResult;
    }
    void Reactivate() override {
        Event("reactivate");
    }
    void SetParent(vgui2::VPANEL) override {
        Event("parent");
    }
    vgui2::VPANEL GetPanel() override {
        return 7;
    }
    void Deactivate() override {
        Event("deactivate");
    }
    void Shutdown() override {
        Event("shutdown");
        if (duringShutdown) {
            duringShutdown();
        }
    }
    void RunFrame() override {
        Event("frame");
    }
} browser;

void *InternalFactory(const char *name, int *) {
    return std::strcmp(name, "VGuiModuleServerBrowser001") == 0 ? &browser : nullptr;
}
namespace cse::server_browser {
CreateInterfaceFn ServerBrowserFactory() {
    return InternalFactory;
}
bool TryRetireServerBrowser() {
    Event("retire");
    return true;
}
bool IsServerBrowserRetired() {
    return true;
}
} // namespace cse::server_browser

CreateInterfaceFn Sys_GetFactory(CSysModule *module) {
    if (!module) {
        ++fakeHandleQueries;
        return nullptr;
    }
    return rejectFactory ? nullptr : InternalFactory;
}
CSysModule *Sys_LoadModule(const char *) {
    ++loads;
    return externalImage ? reinterpret_cast<CSysModule *>(uintptr_t{1}) : nullptr;
}
void Sys_UnloadModule(CSysModule *) {
    ++unloads;
}

struct Taskbar {
    vgui2::VPANEL GetVPanel() {
        return 1;
    }
    void AddTask(vgui2::VPANEL) {
        Event("task");
    }
} taskbar;
Taskbar *g_pTaskbar = &taskbar;
struct FileSystem {
    bool GetLocalPath(const char *source, char *target, int capacity) {
        std::snprintf(target, static_cast<size_t>(capacity), "%s", source);
        return true;
    }
    void GetLocalCopy(const char *) {}
} filesystem;
struct VGui {
    void DPrintf2(const char *, ...) {}
    void PostMessage(vgui2::VPANEL, KeyValues *message, vgui2::VPANEL) {
        Event("quit-message");
        message->deleteThis();
    }
} vgui;
namespace vgui2 {
FileSystem *filesystem() {
    return &::filesystem;
}
VGui *ivgui() {
    return &::vgui;
}
} // namespace vgui2

// Fixture storage for the complete extracted loader methods, not a replacement implementation.
// Extra proposed record fields allow the same behavior cases to observe the native internal record.
class CVGuiSystemModuleLoader {
public:
    ~CVGuiSystemModuleLoader() {
        if (m_pPlatformModuleData) {
            m_pPlatformModuleData->deleteThis();
        }
    }
    enum class ModuleOwnership { ExternalImage, InternalGameUI };
    enum class ModulePhase { Loaded, Initialized, PostInitialized, Attached, Retiring, Retired, Shutdown };
    struct module_t {
        CSysModule *module = nullptr;
        IVGuiModule *moduleInterface = nullptr;
        KeyValues *data = nullptr;
        ModuleOwnership ownership = ModuleOwnership::ExternalImage;
        CSysModule *image = nullptr;
        CreateInterfaceFn factory = nullptr;
        ModulePhase phase = ModulePhase::Loaded;
        bool (*tryRetire)() = nullptr;
        bool (*isRetired)() = nullptr;
        bool initializeEntered = false;
        bool initializeSucceeded = false;
        bool postInitializeEntered = false;
    };
    CreateInterfaceFn GetModuleFactory(int);
    bool InitializeAllModules(CreateInterfaceFn *, int);
    bool LoadPlatformModules(CreateInterfaceFn *, int, bool);
    void ShutdownPlatformModules();
    bool TryRetireNativeModules();
    void UnloadPlatformModules();
    void RunFrame();
    void DeactivatePlatformModules();
    void ReactivatePlatformModules();
    void PostMessageToAllModules(KeyValues *);
    size_t m_nativeCallDepth = 0;
    bool m_bModulesInitialized = false;
    bool m_bPlatformShouldRestartAfterExit = false;
    CUtlVector<module_t> m_Modules;
    KeyValues *m_pPlatformModuleData = nullptr;
};

#include "loader_guard.inc"
#include "loader.inc"

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        CVGuiSystemModuleLoader loader;
        if (name == "reference-selection") {
            externalImage = true;
            Require(loader.LoadPlatformModules(nullptr, 0, false), "reference external module failed");
            Require(loads == 1 && loader.m_Modules.Count() == 1, "reference path selected internal browser");
            loader.ShutdownPlatformModules();
            loader.UnloadPlatformModules();
            Require(unloads == 1, "reference external image did not unload");
        } else if (name == "native-selection") {
            const bool loaded = loader.LoadPlatformModules(nullptr, 0, false);
            Require(loaded, "native Servers record failed without an external serverbrowser image");
            Require(loads == 0, "native Servers selection attempted foreign dynamic module load");
            Require(loader.m_Modules.Count() == 1, "native Servers did not create exactly one record");
        } else if (name == "exact-selection" || name == "external-factory-failure") {
            recordName = "servers";
            externalImage = name == "external-factory-failure";
            rejectFactory = externalImage;
            Require(!loader.LoadPlatformModules(nullptr, 0, false), "nonexact record was selected internally");
            Require(loads == 1 && loader.m_Modules.Count() == 0, "external route was bypassed");
            Require(!externalImage || unloads == 1, "rejected external factory leaked its loaded image");
        } else {
            const int index = loader.m_Modules.AddToTail();
            auto &record = loader.m_Modules[index];
            record.moduleInterface = &browser;
            record.ownership = CVGuiSystemModuleLoader::ModuleOwnership::InternalGameUI;
            record.factory = InternalFactory;
            record.tryRetire = cse::server_browser::TryRetireServerBrowser;
            record.isRetired = cse::server_browser::IsServerBrowserRetired;
            if (name == "internal-factory") {
                Require(loader.GetModuleFactory(index) == InternalFactory,
                        "internal record factory was resolved through a fabricated image handle");
                Require(fakeHandleQueries == 0, "internal factory queried a null image");
            } else if (name == "internal-unload") {
                loader.UnloadPlatformModules();
                Require(unloads == 0, "internal GameUI-owned record was passed to dynamic unload");
            } else if (name == "failed-initialize") {
                initializeResult = false;
                Require(!loader.InitializeAllModules(nullptr, 0), "module initialization failure was accepted");
                Require(std::find(events.begin(), events.end(), "parent") == events.end(),
                        "failed module was attached to live taskbar");
                Require(!loader.m_bModulesInitialized, "failed module loader published platform ready");
            } else if (name == "failed-post") {
                postInitializeResult = false;
                Require(!loader.InitializeAllModules(nullptr, 0), "post-initialize failure was accepted");
                loader.DeactivatePlatformModules();
                loader.ReactivatePlatformModules();
                loader.PostMessageToAllModules(new KeyValues("PendingMessage"));
                Require(std::find(events.begin(), events.end(), "parent") == events.end() &&
                            std::find(events.begin(), events.end(), "reactivate") == events.end() &&
                            std::find(events.begin(), events.end(), "deactivate") == events.end() &&
                            std::find(events.begin(), events.end(), "quit-message") == events.end(),
                        "failed module received published-panel actions");
            } else if (name == "duplicate-initialize") {
                Require(loader.InitializeAllModules(nullptr, 0), "initial initialization failed");
                Require(loader.InitializeAllModules(nullptr, 0), "already attached module lost readiness");
                Require(std::count(events.begin(), events.end(), "initialize") == 1 &&
                            std::count(events.begin(), events.end(), "post-initialize") == 1 &&
                            std::count(events.begin(), events.end(), "task") == 1,
                        "repeated initialize duplicated ownership");
            } else if (name == "shutdown-reentry") {
                Require(loader.InitializeAllModules(nullptr, 0), "setup initialize failed");
                duringShutdown = [&] {
                    loader.UnloadPlatformModules();
                    Require(loader.m_Modules.Count() == 1, "reentrant unload discarded an active module callback");
                };
                loader.ShutdownPlatformModules();
                duringShutdown = {};
                loader.UnloadPlatformModules();
                Require(loader.m_Modules.Count() == 0, "safe outer unload did not resume");
            } else if (name == "unstarted-shutdown") {
                loader.ShutdownPlatformModules();
                Require(std::find(events.begin(), events.end(), "shutdown") == events.end() &&
                            std::find(events.begin(), events.end(), "deactivate") == events.end(),
                        "shutdown called an instance whose initialization was never entered");
            } else {
                throw std::runtime_error("unknown loader case");
            }
        }
        if (loader.m_pPlatformModuleData) {
            loader.m_pPlatformModuleData->deleteThis();
            loader.m_pPlatformModuleData = nullptr;
        }
    });
}
