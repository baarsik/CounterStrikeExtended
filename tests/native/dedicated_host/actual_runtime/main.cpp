#include "dedicated/native/DedicatedSession.h"
#include "launcher/native/MacRuntimeSession.h"
#include <cstdio>
#include <cstring>
#include <mach-o/dyld.h>
#include <string>

namespace {
bool CohortUnmapped(int cycle) {
    bool absent = true;
    unsigned int cohortMappings = 0;
    unsigned int sdkMappings = 0;
    for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
        const char* image = _dyld_get_image_name(index);
        const char* separator = image ? std::strrchr(image, '/') : nullptr;
        const char* name = separator ? separator + 1 : image;
        if (name && (std::strcmp(name, "swds.dylib") == 0 || std::strcmp(name, "vgui.dylib") == 0
            || std::strcmp(name, "filesystem_stdio.dylib") == 0)) {
            std::fprintf(stderr, "AFTER_CLOSE_MAPPED_IMAGE cycle=%d path=%s\n", cycle, image);
            ++cohortMappings;
            absent = false;
        }
        if (name && std::strcmp(name, "libsteam_api.dylib") == 0) {
            ++sdkMappings;
        }
    }
    std::printf("DYLD_AFTER_CLOSE cycle=%d software_font_filesystem=%u steam_sdk=%u\n",
        cycle, cohortMappings, sdkMappings);
    return absent;
}
}

int main(int argc, char** argv) {
    constexpr int ModuleCycles = 3;
    const bool moduleOnly = argc == 3 && std::strcmp(argv[1], "--module-only") == 0;
    const bool clientNoRun = argc == 3 && std::strcmp(argv[1], "--client-no-run") == 0;
    std::uint64_t previousGeneration = 0;
    for (int cycle = 0; cycle < ModuleCycles; ++cycle) {
        std::string error;
        if (clientNoRun) {
            cse_mac_runtime::Session session;
            const std::string enginePath = std::string(argv[2]) + "/swds.dylib";
            const std::string fontPath = std::string(argv[2]) + "/vgui.dylib";
            void* image = session.Load(enginePath.c_str(), fontPath.c_str(), cse_mac_runtime::EngineImage::Software);
            bool adopted = false;
            if (!image || !session.Open(image, enginePath.c_str(), fontPath.c_str(), adopted)) {
                if (image && !adopted) {
                    dlclose(image);
                }
                std::fprintf(stderr, "CLIENT_ROOT_OPEN_FAILED cycle=%d\n", cycle);
                return 5;
            }
            if (!session.Close() || !CohortUnmapped(cycle)) {
                return 6;
            }
            std::printf("ACTUAL_CLIENT_ROOT_UNMAPPED cycle=%d no_engine_run=1\n", cycle);
        } else if (moduleOnly) {
            cse::dedicated::DedicatedModuleSet modules;
            if (!modules.Open(argv[2], false, error)) {
                std::fprintf(stderr, "MODULE_OPEN_FAILED cycle=%d: %s\n", cycle, error.c_str());
                return 1;
            }
            modules.Close();
            std::printf("MODULE_OWNER_AFTER_CLOSE cycle=%d closed=%d\n", cycle, modules.IsClosed());
            if (!modules.IsClosed() || !CohortUnmapped(cycle)) {
                return 2;
            }
            std::printf("ACTUAL_MODULE_UNMAPPED cycle=%d no_session_init=1\n", cycle);
        } else {
            cse::dedicated::DedicatedSession session;
            if (!session.Open(argc, argv, error)) {
                std::fprintf(stderr, "ACTUAL_SESSION_OPEN_FAILED cycle=%d: %s\n", cycle, error.c_str());
                return 3;
            }
            std::printf("ACTUAL_SESSION_OPEN cycle=%d generation=%llu\n", cycle,
                static_cast<unsigned long long>(session.Generation()));
            std::fflush(stdout);
            const int result = session.Run();
            std::printf("ACTUAL_SESSION_FINISHED cycle=%d result=%d closed=%d generation=%llu\n", cycle, result,
                session.CurrentState() == cse::dedicated::DedicatedSession::State::Closed,
                static_cast<unsigned long long>(session.Generation()));
            std::fflush(stdout);
            if (result != 0 || session.Generation() <= previousGeneration || !CohortUnmapped(cycle)) {
                return 4;
            }
            previousGeneration = session.Generation();
            std::printf("ACTUAL_SESSION_UNMAPPED cycle=%d\n", cycle);
        }
        std::fflush(stdout);
    }
    return 0;
}
