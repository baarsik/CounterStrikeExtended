#include "native_module.h"
#include "native_runtime_host.h"
#include "VGui1HostImageOwner.h"
#include <cstdio>
#include <cstdlib>
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
    Require(argc == 2 || argc == 3, "runtime directory and optional engine image");
    const std::string engine = std::string(argv[1]) + "/" + (argc == 3 ? argv[2] : "hw.dylib");
    const std::string font = std::string(argv[1]) + "/vgui.dylib";
    void* image = cse_platform::LoadModule(engine.c_str(), cse_platform::ModuleFallback::None);
    Require(image != nullptr, "load actual engine root");
    const auto open = reinterpret_cast<cse::native_runtime::Open>(
        cse_platform::GetSymbol(image, cse::native_runtime::OpenSymbol));
    const auto close = reinterpret_cast<cse::native_runtime::Close>(
        cse_platform::GetSymbol(image, cse::native_runtime::CloseSymbol));
    Require(open && close, "actual engine publishes checked native bootstrap entries");
    cse_vgui1_host::ImageOwner owner;
    const cse_vgui1_host::AdmittedImageV1 admitted{sizeof(admitted), cse_vgui1_host::AdmittedImageVersion,
        1, image, engine.c_str()};
    Require(owner.Open(admitted, cse_vgui1_host::AnchorMode::AdoptInitialReference, image)
        == cse_vgui1_host::Result::Ready, "executable adopts actual root reference");
    Require(owner.EnterRoot() == cse_vgui1_host::Result::Ready, "enter retained root call");
    cse::native_runtime::BindingV1 binding{sizeof(binding), cse::native_runtime::BindingVersion,
        1, engine.c_str(), font.c_str(), owner.Contract()};
    Require(open(nullptr) != cse::native_runtime::Ready, "reject missing binding");
    ++binding.version;
    Require(open(&binding) != cse::native_runtime::Ready, "reject foreign binding version");
    --binding.version;
    const char* correctEngine = binding.enginePath;
    binding.enginePath = font.c_str();
    Require(open(&binding) != cse::native_runtime::Ready, "reject a binding for a different loaded image");
    binding.enginePath = correctEngine;
    Require(open(&binding) == cse::native_runtime::Ready, "open native root with genuine anchored ownership");
    Require(open(&binding) != cse::native_runtime::Ready, "reject double open");
    Require(close() == cse::native_runtime::Closed, "close actual native root before executable release");
    Require(close() == cse::native_runtime::Closed, "idempotent root close");
    Require(owner.LeaveRoot() == cse_vgui1_host::Result::Ready, "return from all root code");
    cse_vgui1_host::Statistics statistics{};
    Require(owner.Inspect(statistics) == cse_vgui1_host::Result::Ready, "read retained-reference counts");
    Require(statistics.retainedReferences == 1 && statistics.acceptedTransfers >= 1,
        "root callbacks balance extra references while executable retains its anchor");
    Require(owner.Close() == cse_vgui1_host::Result::Closed, "release executable root after quiescence");
}
