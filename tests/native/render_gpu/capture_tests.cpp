#include "gpu_capture.h"
#include <cstdio>
#include <cstdlib>

using namespace cse::render;

namespace {
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL GPU packet preparation: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    DrawPacket draw;
    draw.raster.viewport = {{0, 0, 64, 64}};
    draw.vertices.resize(3);
    draw.vertices[0].position = {{-2, -2, 0, 1}};
    draw.vertices[1].position = {{2, -2, 0, 1}};
    draw.vertices[2].position = {{0, 2, 0, 1}};
    draw.modelView.values[0] = 0.5f;
    draw.modelView.values[5] = 0.5f;
    GpuDrawData prepared;
    Check(PrepareGpuDraw(draw, prepared) == GpuStatus::Ok, "valid smooth triangle");
    Check(prepared.vertex.modelView.values[0] == 0.5f, "actual captured matrix retained");
    Check(prepared.vertices[0].position[0] == -2 && prepared.vertices[0].color[0] == 1,
        "canonical input position/color retained");
    draw.vertices[0].color[0] = 0;
    Check(prepared.vertices[0].color[0] == 1, "prepared bytes owned");
    draw.shade = Shade::Flat;
    Check(PrepareGpuDraw(draw, prepared) == GpuStatus::Unsupported, "flat convention remains unqualified");
    draw.shade = Shade::Smooth;
    draw.primitive = Primitive::Quads;
    Check(PrepareGpuDraw(draw, prepared) == GpuStatus::Unsupported, "no invented quad lowering");
    std::puts("PASS GPU packet preparation; no GPU execution");
}
