#pragma once

#include "../public/native_restart_command_line.h"

namespace cse_restart {
// Only explicit video commands set this intent. A plain _restart retains the
// current launch arguments, while a menu change replaces the requested groups.
class VideoSettings {
public:
    void SetRenderer(int hardware, bool windowed) {
        rendererChanged = true;
        selectedHardware = hardware;
        selectedWindowed = windowed;
    }

    void SetSize(int width, int height) {
        sizeChanged = true;
        selectedWidth = width;
        selectedHeight = height;
    }

    void Reset() {
        *this = VideoSettings{};
    }

    bool Append(char* output, std::size_t capacity, const std::string& resumeCommands = {}) {
        if (!rendererChanged && !sizeChanged && resumeCommands.empty()) {
            return true;
        }
        if (!output) {
            return false;
        }
        std::size_t used = 0;
        while (used < capacity && output[used]) {
            ++used;
        }
        std::string generated = resumeCommands;
        if (rendererChanged) {
            constexpr int Direct3DRenderer = 2;
            generated += selectedHardware == 0 ? " -soft" :
                selectedHardware == Direct3DRenderer ? " -d3d" : " -gl";
            generated += selectedWindowed ? " -windowed" : " -fullscreen";
        }
        if (sizeChanged) {
            generated += " -w " + std::to_string(selectedWidth) + " -h " + std::to_string(selectedHeight);
        }
        // Validate the entire append before writing; never produce a truncated
        // command or drop one group while silently retaining another old group.
        if (used == capacity || generated.size() >= capacity - used) {
            return false;
        }
        std::memcpy(output + used, generated.c_str(), generated.size() + 1);
        Reset();
        return true;
    }

private:
    bool rendererChanged = false;
    bool sizeChanged = false;
    int selectedHardware = 0;
    bool selectedWindowed = false;
    int selectedWidth = 0;
    int selectedHeight = 0;
};
}
