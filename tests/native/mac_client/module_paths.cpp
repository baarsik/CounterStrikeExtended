#include "FileSystem.h"
#include "native_filesystem_module.h"
#include "runtime_module_names.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "supply the real runtime directory");
    const auto root = std::filesystem::canonical(argv[1]);
    const auto image = root / cse::runtime_modules::FileSystem;
    void* module = cse_platform::LoadModule(image.c_str(), cse_platform::ModuleFallback::None);
    Require(module != nullptr, "load actual filesystem");
    const auto factory = cse::filesystem_abi::CheckedFactory(module);
    Require(factory != nullptr, "validate owned filesystem before callback exchange");
    auto* files = static_cast<IFileSystem*>(factory("VFileSystem009", nullptr));
    Require(files != nullptr, "filesystem interface");
    files->Mount();
    files->AddSearchPath(root.c_str(), "ROOT");
    files->AddSearchPath((root / "valve").c_str(), "DEFAULTGAME");
    // The file need not be in the active game. Resolve its native name through
    // the same search interface consumed by COM_ExpandFilename.
    char resolved[4096]{};
    Require(files->GetLocalPath(cse::runtime_modules::FileSystem, resolved, sizeof(resolved)) != nullptr,
        "native filesystem name resolves through actual VFS");
    Require(std::filesystem::equivalent(resolved, image), "filesystem resolves to the owned root image");
    Require(files->GetLocalPath(cse::runtime_modules::GameUI, resolved, sizeof(resolved)) != nullptr,
        "native GameUI name resolves through the fallback game");
    Require(std::filesystem::equivalent(resolved, root / "valve/cl_dlls/GameUI.dylib"),
        "GameUI resolves to the owned fallback image");
    Require(files->GetLocalPath("filesystem_stdio.dll", resolved, sizeof(resolved)) == nullptr,
        "missing foreign filename must not be mistaken for a native VFS file");
    files->Unmount();
    cse_platform::UnloadModule(module);
}
