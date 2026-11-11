#include "DedicatedModuleSet.h"
#include "../../launcher/native/MacRuntimeSession.h"
#include "../../public/native_dedicated_abi.h"
#include "../../public/native_filesystem_module.h"
#include <cstdio>
#include <cstdlib>
#include "../../public/runtime_module_names.h"
#if defined(CSE_NATIVE_RUNTIME_BOOTSTRAP)
#include "../../public/native_standalone_abi.h"
#include "../../public/native_standalone_cohort.h"
#include "cse_standalone_cohort_images.h"
#endif

namespace cse::dedicated {
DedicatedModuleSet::~DedicatedModuleSet() noexcept {
    Close();
    if (!IsClosed()) {
        std::fputs("Dedicated provider did not retire; refusing unsafe process teardown\n", stderr);
        std::_Exit(EXIT_FAILURE);
    }
}

bool DedicatedModuleSet::Open(const char* root, bool gui, std::string& error) {
    if (!IsClosed() || !root || root[0] != '/') {
        error = "Dedicated modules require a closed owner and an absolute runtime root";
        return false;
    }
    shutdown_ = false;
    retired_ = false;
    shutdownResult_ = 0;
    const std::string base(root);
    if (gui) {
#if defined(CSE_NATIVE_RUNTIME_BOOTSTRAP)
        if (!standalone_cohort::Admit((base + "/" + runtime_modules::Vgui2).c_str(),
                                     (base + "/" + runtime_modules::FileSystem).c_str(),
                                     cse_standalone_images::VguiSha256, cse_standalone_images::FilesystemSha256)) {
            error = "Standalone VGUI/filesystem cohort mismatch before module constructors";
            return false;
        }
#else
        error = "Standalone VGUI requires the owning-build physical and image cohort";
        return false;
#endif
    }
    const std::string enginePath = base + "/" + runtime_modules::SoftwareEngine;
    const std::string fontPath = base + "/" + runtime_modules::Vgui1;
    if (!cse_mac_runtime::Session::Admit(enginePath.c_str(), fontPath.c_str(),
                                         cse_mac_runtime::EngineImage::Software)) {
        error = "Software engine/VGUI cohort mismatch before module constructors";
        return false;
    }
    engineModule_ = cse_mac_runtime::Session::LoadAdmitted(enginePath.c_str(), fontPath.c_str(),
        cse_mac_runtime::EngineImage::Software, fontModule_);
    if (!engineModule_) {
        error = "Cannot load admitted software engine";
        return false;
    }
    // Descriptor and mandatory final-retirement boundary precede every factory.
    retire_ = reinterpret_cast<CseNativeDedicatedRetireFn>(
        cse_platform::GetSymbol(engineModule_, "CseNativeDedicatedRetireV1"));
    if (!retire_ || !dedicated_abi::Compatible(engineModule_, dedicated_abi::Symbol, dedicated_abi::Engine) ||
        !dedicated_abi::Compatible(engineModule_, dedicated_abi::AdminSymbol, dedicated_abi::Admin)) {
        error = "Missing or incompatible dedicated API, admin role, or final-retirement boundary";
        // No Init/factory call has occurred. Constructors are already admitted by
        // the exact software cohort; the dedicated transport has never started.
        cse_platform::UnloadModule(engineModule_);
        engineModule_ = nullptr;
        retire_ = nullptr;
        return false;
    }
    if (!software_.Open(engineModule_, enginePath.c_str(), software_lifetime::Mode::Dedicated)) {
        error = "Missing or incompatible whole-software retirement owner";
        Close();
        return false;
    }
    filesystemModule_ =
        cse_platform::LoadModule((base + "/" + runtime_modules::FileSystem).c_str(), cse_platform::ModuleFallback::None);
    filesystemFactory_ = filesystem_abi::CheckedFactory(filesystemModule_);
    engineFactory_ = reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(engineModule_, "CreateInterface"));
    if (!filesystemFactory_ || !engineFactory_) {
        error = "Missing admitted engine/filesystem factory";
        Close();
        return false;
    }
#if defined(CSE_NATIVE_RUNTIME_BOOTSTRAP)
    if (gui) {
        // An admitted standalone provider is private to this owner. Exporting its
        // symbols globally can retain unrelated dyld bindings after Close.
        vguiModule_ = dlopen((base + "/" + runtime_modules::Vgui2).c_str(), RTLD_NOW | RTLD_LOCAL);
        vguiFactory_ = standalone_abi::CheckedFactory(vguiModule_);
        if (!vguiFactory_ || !standalone_abi::ValidateViews(vguiFactory_, filesystemFactory_)) {
            error = "Standalone VGUI role or complete physical factory views rejected";
            Close();
            return false;
        }
    }
#endif
    auto* engine = static_cast<IDedicatedServerAPI*>(engineFactory_("VENGINE_HLDS_API_VERSION002", nullptr));
    auto* admin = static_cast<IGameServerData*>(engineFactory_("GameServerData001", nullptr));
    auto* filesystem = static_cast<IFileSystem*>(filesystemFactory_("VFileSystem009", nullptr));
    constexpr std::size_t DedicatedMethods = 5;
    constexpr std::size_t AdminMethods = 2;
    if (!dedicated_abi::ViewCompatible<IDedicatedServerAPI>(engine, engineFactory_, DedicatedMethods) ||
        !dedicated_abi::ViewCompatible<IGameServerData>(admin, engineFactory_, AdminMethods) ||
        !dedicated_abi::ViewCompatible<IFileSystem>(filesystem, filesystemFactory_, filesystem_abi::PrefixMethods)) {
        error = "Admitted provider returned an invalid interface view or foreign physical slots";
        Close();
        return false;
    }
    engine_ = engine;
    admin_ = admin;
    filesystem_ = filesystem;
    return true;
}

bool DedicatedModuleSet::Initialize(char* base, char* command, CreateInterfaceFn host) {
    software_lifetime::ImageOwner::Call call(&software_);
    return call && engine_ && engine_->Init(base, command, host, filesystemFactory_);
}

bool DedicatedModuleSet::Frame() {
    software_lifetime::ImageOwner::Call call(&software_);
    return call && engine_ && engine_->RunFrame();
}

void DedicatedModuleSet::AddText(char* text) {
    software_lifetime::ImageOwner::Call call(&software_);
    if (call && engine_) {
        engine_->AddConsoleText(text);
    }
}

void DedicatedModuleSet::Status(float* fps, int* active, int* maximum, char* map) {
    software_lifetime::ImageOwner::Call call(&software_);
    if (call && engine_) {
        engine_->UpdateStatus(fps, active, maximum, map);
    }
}

int DedicatedModuleSet::Shutdown() noexcept {
    if (engine_ && !shutdown_) {
        software_lifetime::ImageOwner::Call call(&software_);
        if (!call) {
            return shutdownResult_;
        }
        shutdownResult_ = engine_->Shutdown();
        shutdown_ = true;
    }
    return shutdownResult_;
}

bool DedicatedModuleSet::Retire() noexcept {
    if (retired_) {
        return true;
    }
    Shutdown();
    if (!software_.Retire()) {
        return false;
    }
    retired_ = true;
    return true;
}

void DedicatedModuleSet::Close() noexcept {
    if (!Retire()) {
        return;
    }
    engine_ = nullptr;
    admin_ = nullptr;
    engineFactory_ = nullptr;
    retire_ = nullptr;
    cse_platform::UnloadModule(engineModule_);
    engineModule_ = nullptr;
    cse_platform::UnloadModule(fontModule_);
    fontModule_ = nullptr;
    cse_platform::UnloadModule(vguiModule_);
    vguiModule_ = nullptr;
    vguiFactory_ = nullptr;
    filesystem_ = nullptr;
    filesystemFactory_ = nullptr;
    cse_platform::UnloadModule(filesystemModule_);
    filesystemModule_ = nullptr;
}
} // namespace cse::dedicated
