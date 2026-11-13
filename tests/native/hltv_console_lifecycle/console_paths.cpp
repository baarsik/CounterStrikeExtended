// Compile the complete owner with its real flags; only rename its process entry point.
#define main ProductionConsoleMain
#include "../../../GoldSrc-SDK/HLTV/Console/System.cpp"
#undef main

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <vector>

namespace {

bool Check(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
    return condition;
}

std::vector<int> OpenDescriptors() {
    // Console/filesystem descriptors allocated by this isolated fixture stay below this bound.
    constexpr int DescriptorProbeLimit = 256;
    std::vector<int> descriptors;
    for (int descriptor = 0; descriptor < DescriptorProbeLimit; ++descriptor) {
        if (fcntl(descriptor, F_GETFD) >= 0) {
            descriptors.push_back(descriptor);
        }
    }
    return descriptors;
}

class ReentrantModule : public BaseSystemModule {
public:
    int shutdowns = 0;

    void ShutDown() override {
        ++shutdowns;
        m_System->ShutDown();
        BaseSystemModule::ShutDown();
    }
};

class InputEndedFrameProbe : public BaseSystemModule {
public:
    static constexpr int RequiredFrames = 3;
    int frames = 0;

    void RunFrame(double time) override {
        BaseSystemModule::RunFrame(time);
        ++frames;
        if (frames >= RequiredFrames) {
            m_System->Stop();
        }
    }
};

bool WriteContinuityStartup(const char* commands) {
    auto* file = std::fopen("valve/eof-continuity.cfg", "w");
    if (!Check(file != nullptr, "create private startup configuration")) {
        return false;
    }
    const bool written = std::fputs(commands, file) >= 0;
    const bool closed = std::fclose(file) == 0;
    return Check(written && closed, "write private startup configuration");
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    const auto descriptors = OpenDescriptors();
    if (std::strcmp(argv[1], "preconsole") == 0) {
        System system;
        constexpr std::size_t OversizedNameCapacity = 512;
        char oversizedName[OversizedNameCapacity];
        std::memset(oversizedName, 'n', sizeof(oversizedName) - 1);
        oversizedName[sizeof(oversizedName) - 1] = 0;
        bool valid = Check(!system.Init(&system, 0, oversizedName), "name failure must fail before Console Init");
        system.ShutDown();
        system.ShutDown();
        valid = Check(system.GetFileSystem() == nullptr, "pre-console cleanup has no filesystem") && valid;
        valid = Check(OpenDescriptors() == descriptors, "pre-console failure must preserve descriptors") && valid;
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    const bool early = std::strcmp(argv[1], "early") == 0;
    const bool partial = std::strcmp(argv[1], "partial") == 0;
    // Configure exact owned process arguments through the production entry point once.
    char executable[] = "console_paths";
    char ip[] = "-ip";
    char loopback[] = "127.0.0.1";
    char port[] = "-port";
    char ephemeral[] = "0";
    char quit[] = "+quit";
    char execute[] = "+exec";
    char startup[] = "eof-continuity.cfg";
    const bool continuity = std::strcmp(argv[1], "eof-continuity") == 0;
    char* arguments[] = {executable, ip, loopback, port, ephemeral, continuity ? execute : quit, startup};
    const int argumentCount = static_cast<int>(sizeof(arguments) / sizeof(arguments[0])) - (continuity ? 0 : 1);
    if (continuity && !WriteContinuityStartup("quit\n")) {
        return EXIT_FAILURE;
    }
    if (early || partial) {
        const int result = ProductionConsoleMain(argumentCount, arguments);
        bool valid = Check(result == EXIT_FAILURE, "failed Init must fail process entry point");
        valid = Check(g_System.GetFileSystem() == nullptr, "failed Init must retire filesystem") && valid;
        g_System.ShutDown();
        g_System.ShutDown();
        valid = Check(OpenDescriptors() == descriptors, "failed Init must retire owned descriptors") && valid;
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    char name[] = "Console";
    if (ProductionConsoleMain(argumentCount, arguments) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    // Init replays startup commands. Only the bootstrap lifetime may quit before frame processing.
    if (continuity && !WriteContinuityStartup("echo continuity-start\n")) {
        return EXIT_FAILURE;
    }
    if (!g_System.Init(&g_System, 0, name)) {
        return EXIT_FAILURE;
    }
    bool valid = true;
    if (std::strcmp(argv[1], "eof-continuity") == 0) {
        InputEndedFrameProbe probe;
        char probeName[] = "input-ended-frame-probe";
        valid = Check(g_System.AddModule(&probe, probeName), "add frame-continuity probe") && valid;
        g_System.RunFrame(0);
        valid = Check(probe.frames >= InputEndedFrameProbe::RequiredFrames,
                      "stdin EOF must preserve frame processing until explicit Stop") && valid;
        g_System.ShutDown();
        valid = Check(g_System.GetFileSystem() == nullptr, "explicit Stop retires filesystem") && valid;
        valid = Check(OpenDescriptors() == descriptors, "explicit Stop retires owned descriptors") && valid;
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }
#ifdef CONTROLLED_VGUI_SERVICES
    if (std::strncmp(argv[1], "vgui-", std::strlen("vgui-")) == 0) {
        if (std::strcmp(argv[1], "vgui-success") != 0) {
            setenv("CSE_CONSOLE_VGUI_FAIL_SCHEME", "1", 1);
            valid = Check(g_System.GetPanel() == nullptr, "scheme failure must fail actual owner startup") && valid;
        }
        if (std::strcmp(argv[1], "vgui-failure") != 0) {
            unsetenv("CSE_CONSOLE_VGUI_FAIL_SCHEME");
            valid = Check(g_System.GetPanel() != nullptr,
                          "actual owner must support successful startup/retry") && valid;
        }
        {
            cse::modules::CallbackScope callback(&g_System);
            g_System.ShutDown();
            std::fprintf(stderr, "VGUI owner-deferred\n");
        }
        g_System.ShutDown();
        g_System.ShutDown();
        valid = Check(g_System.GetFileSystem() == nullptr, "VGUI owner must complete retirement") && valid;
        valid = Check(OpenDescriptors() == descriptors, "VGUI cleanup must retire owned descriptors") && valid;
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }
#endif
    if (std::strcmp(argv[1], "deferred") == 0) {
        ReentrantModule module;
        char moduleName[] = "reentrant";
        valid = Check(g_System.AddModule(&module, moduleName), "add borrowed reentry probe") && valid;
        {
            cse::modules::CallbackScope callback(&g_System);
            g_System.ShutDown();
            valid = Check(module.shutdowns == 1, "reentry must shut down borrowed probe exactly once") && valid;
            valid = Check(g_System.GetFileSystem() != nullptr,
                          "callback must defer provider/filesystem unload") && valid;
            valid = Check(!g_System.Init(&g_System, 0, name), "Init must reject pending retirement") && valid;
        }
        g_System.ShutDown();
        valid = Check(module.shutdowns == 1, "retry must not repeat module cleanup") && valid;
    } else {
        g_System.ShutDown();
    }
    valid = Check(g_System.GetFileSystem() == nullptr, "completed cleanup clears filesystem") && valid;
    g_System.ShutDown();
    g_System.ShutDown();
    valid = Check(g_System.GetFileSystem() == nullptr, "repeated cleanup keeps filesystem retired") && valid;
    valid = Check(OpenDescriptors() == descriptors, "completed cleanup must retire owned descriptors") && valid;
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
