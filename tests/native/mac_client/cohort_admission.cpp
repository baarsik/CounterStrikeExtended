#include "MacRuntimeSession.h"
#include <cassert>
#include <string>
#include <dlfcn.h>

int main(int argc, char** argv) {
    assert(argc == 3 || argc == 4);
    const std::string root = argv[1];
    const bool software = std::string(argv[2]) == "software";
    using cse_mac_runtime::EngineImage;
    using cse_mac_runtime::Session;
    const std::string font = root + "/vgui.dylib";
    const std::string hardware = root + "/hw.dylib";
    const std::string sw = root + "/swds.dylib";
    if (argc == 4) {
        void* foreign = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
        assert(foreign);
        assert(!Session::Admit(hardware.c_str(), font.c_str(), EngineImage::Hardware));
        assert(!Session::Admit(sw.c_str(), font.c_str(), EngineImage::Software));
        assert(dlclose(foreign) == 0);
        return 0;
    }
    assert(Session::Available(EngineImage::Hardware));
    assert(Session::Available(EngineImage::Software) == software);
    assert(Session::Admit(hardware.c_str(), font.c_str(), EngineImage::Hardware));
    assert(Session::Admit(sw.c_str(), font.c_str(), EngineImage::Software) == software);
    assert(!Session::Admit(sw.c_str(), font.c_str(), EngineImage::Hardware));
    assert(!Session::Admit(hardware.c_str(), font.c_str(), EngineImage::Software));
    assert(!Session::Admit((root + "/mixed.dylib").c_str(), font.c_str(), EngineImage::Software));
    assert(!Session::Admit((root + "/missing.dylib").c_str(), font.c_str(), EngineImage::Software));
    assert(!Session::Admit(sw.c_str(), (root + "/mixed.dylib").c_str(), EngineImage::Software));
    assert(!Session::Admit(hardware.c_str(), (root + "/missing.dylib").c_str(), EngineImage::Hardware));
}
