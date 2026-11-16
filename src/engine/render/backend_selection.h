#pragma once

namespace cse {
namespace render {

enum class Backend { OpenGL, Metal };
struct BackendSelection {
    Backend backend;
    const char* error;
};

constexpr BackendSelection SelectBackend(bool macOS, bool metalCompiled, bool glFlag, bool metalFlag) {
    if (glFlag && metalFlag) {
        return {Backend::OpenGL, "-gl and -metal cannot be used together"};
    }
    if (metalFlag && (!macOS || !metalCompiled)) {
        return {Backend::OpenGL, "Metal rendering is unavailable in this build"};
    }
    return {macOS && metalCompiled && !glFlag ? Backend::Metal : Backend::OpenGL, nullptr};
}

}
}
