#include "frame_module_api.h"
#include <dlfcn.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    void* firstImage = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    void* secondImage = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
    if (!firstImage || !secondImage) {
        std::cerr << "fixture dlopen: " << dlerror() << "\n";
        return 3;
    }
    // dlsym resolves an ordinary POD data object; function pointers are supplied
    // by the real compiler's typed initializer, never cast from raw addresses.
    const auto* first = static_cast<const FrameModuleApi*>(dlsym(firstImage, "cse_test_frame_api_a"));
    const auto* second = static_cast<const FrameModuleApi*>(dlsym(secondImage, "cse_test_frame_api_b"));
    if (!first || !second) {
        return 4;
    }
    int panelIdentity = 0;
    void* firstFrame = first->begin(&panelIdentity, 5);
    void* secondFrame = second->begin(&panelIdentity, 5);
    const bool independent = first->entered(firstFrame) && second->entered(secondFrame);
    void* firstRepeat = first->begin(&panelIdentity, 5);
    void* secondRepeat = second->begin(&panelIdentity, 5);
    const bool localRejection = !first->entered(firstRepeat) && !second->entered(secondRepeat);
    second->end(secondRepeat);
    first->end(firstRepeat);
    second->end(secondFrame);
    first->end(firstFrame);
    dlclose(secondImage);
    dlclose(firstImage);
    std::cout << "independent=" << independent << " local-rejection=" << localRejection << "\n";
    return independent && localRejection ? 0 : 1;
}
