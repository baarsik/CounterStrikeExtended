#include "../../../src/public/radar_renderer_api.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace cse::radar {
std::uint64_t GraphicsGeneration() {
    return 7;
}
}
namespace cse::render {
bool UsingMetal() {
    return false;
}
}
extern "C" void* CreateInterface(const char*, int*);
int main() {
    auto* api = static_cast<CseRadarRendererApi*>(CreateInterface(CSE_RADAR_RENDERER_INTERFACE, nullptr));
    if (!api) {
        return 1;
    }
    const CseRadarTexture handles[] = {{0, 0}, {0, 1}, {7, 0}, {6, 1}, {7, 1}};
    const CseRadarResult expected[] = {CSE_RADAR_OK, CSE_RADAR_INVALID_TEXTURE, CSE_RADAR_INVALID_TEXTURE,
        CSE_RADAR_STALE_EPOCH, CSE_RADAR_UNAVAILABLE};
    for (unsigned index = 0; index < sizeof(handles) / sizeof(handles[0]); ++index) {
        const auto result = api->ReleaseImage(&handles[index]);
        if (result != expected[index]) {
            std::cerr << "ReleaseImage case " << index << ": " << result << " expected " << expected[index] << '\n';
            return 1;
        }
    }
    if (api->ReleaseImage(nullptr) != CSE_RADAR_INVALID_ARGUMENT) {
        return 1;
    }
    std::cout << "public release: null, half-null, stale, current unavailable passed\n";
}
