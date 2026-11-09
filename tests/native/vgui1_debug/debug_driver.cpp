#if defined(CSE_VGUI1_DEBUG_NATIVE)
#include "../../../GoldSrc-SDK/utils/vgui/lib-src/vgui/posix/Debug.cpp"
#else
#include "VGUI.h"
#endif

#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
void Return(int value) {
    std::printf("%d\n", value);
}
}

int main(int argc, char** argv) {
    if (argc < 2 || !std::setlocale(LC_CTYPE, "C")) {
        return 2;
    }
    const std::string mode(argv[1]);
    if (mode == "plain" || mode == "counter") {
        if (argc != 3) {
            return 2;
        }
        const std::size_t count = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
        if (count > 65536) {
            return 2;
        }
        const std::string text(count, 'x');
        Return(mode == "plain" ? vgui::vgui_dprintf("%s", text.c_str()) :
            vgui::vgui_dprintf2("%s", text.c_str()));
    } else if (mode == "values") {
        Return(vgui::vgui_dprintf("[%d][%u][%lld][%zu][%.2f][%s]", -17, 23u, 0x100000001LL,
            static_cast<std::size_t>(31), 1.25, "tail"));
    } else if (mode == "nul") {
        Return(vgui::vgui_dprintf("A%cB", 0));
        Return(vgui::vgui_dprintf2("C%cD", 0));
    } else if (mode == "error") {
        errno = 0;
        Return(vgui::vgui_dprintf("pre%lcpost", static_cast<wint_t>(0xd800)));
        std::printf("errno=%d\n", errno);
        errno = 0;
        Return(vgui::vgui_dprintf2("pre%lcpost", static_cast<wint_t>(0xd800)));
        std::printf("errno=%d\n", errno);
    } else if (mode == "sequence") {
        constexpr int PrefixTransitionCalls = 12;
        for (int index = 0; index < PrefixTransitionCalls; ++index) {
            Return(vgui::vgui_dprintf2("[%d]", index));
        }
    } else if (mode == "prefix-limit") {
        const std::string text(2048, 'p');
        for (int index = 0; index < 12; ++index) {
            Return(vgui::vgui_dprintf2("%s", text.c_str()));
        }
#if defined(CSE_VGUI1_DEBUG_NATIVE)
    } else if (mode == "wrap-positive") {
        CounterStorage() = static_cast<std::uint32_t>(INT32_MAX) - 1;
        for (int index = 0; index < 4; ++index) {
            Return(vgui::vgui_dprintf2("!"));
        }
    } else if (mode == "wrap-limit") {
        CounterStorage() = static_cast<std::uint32_t>(INT32_MAX) + 1;
        const std::string text(4096, 'w');
        Return(vgui::vgui_dprintf2("%s", text.c_str()));
    } else if (mode == "wrap-zero") {
        CounterStorage() = UINT32_MAX - 1;
        for (int index = 0; index < 4; ++index) {
            Return(vgui::vgui_dprintf2("!"));
        }
    } else if (mode == "concurrent") {
        constexpr int Threads = 8;
        constexpr int RecordsPerThread = 128;
        constexpr std::size_t PayloadBytes = 256;
        const std::string payload(PayloadBytes, 'z');
        std::vector<std::thread> threads;
        for (int thread = 0; thread < Threads; ++thread) {
            threads.emplace_back([thread, &payload]() {
                for (int index = 0; index < RecordsPerThread; ++index) {
                    vgui::vgui_dprintf("P[%d,%d]%s;", thread, index, payload.c_str());
                    vgui::vgui_dprintf2("C[%d,%d]%s;", thread, index, payload.c_str());
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        std::printf("plain=%d counter=%d\n", Threads * RecordsPerThread, Threads * RecordsPerThread);
#endif
    } else {
        return 2;
    }
    return 0;
}
