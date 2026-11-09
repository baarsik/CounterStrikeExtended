#include "fixture_platform.h"
#include "fixture_declarations.inc"

// Separate translation unit: fail if these unselected device/propagation paths
// are reached, without teaching the compiler that actual production code cannot return.
void CA3d::BindMaterial(void*) {
    std::fputs("Unexercised A3D material binding reached\n", stderr);
    std::abort();
}

void CA3dOpening::Render(CA3d*) {
    std::fputs("Unexercised A3D opening propagation reached\n", stderr);
    std::abort();
}
