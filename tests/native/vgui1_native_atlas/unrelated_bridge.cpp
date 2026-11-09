#include "VGui1NativeRuntime.h"
#include <cstdlib>
namespace cse_vgui1_native {
bool UploadEngineFontPage(IEngineSurface*, int, const unsigned char*, std::size_t) noexcept {
    std::abort();
}
bool RetireEngineFontPage(IEngineSurface*, int, bool) noexcept {
    std::abort();
}
}
