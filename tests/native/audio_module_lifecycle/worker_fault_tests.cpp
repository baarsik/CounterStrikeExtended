#include "worker_fault_injection.h"
#include "../../../GoldSrc-SDK/engine/thread.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int initializations = 0;
int shutdowns = 0;
}

CCDAudio::CCDAudio() {}
CCDAudio::~CCDAudio() {}
int CCDAudio::Init() {
    ++initializations;
    return 0;
}
void CCDAudio::Shutdown() {
    ++shutdowns;
}
void CCDAudio::Play(int, bool) {}
void CCDAudio::PlayFile(const char*, bool) {}
void CCDAudio::Pause() {}
void CCDAudio::Resume() {}
void CCDAudio::Frame() {}
void CCDAudio::MP3_Resume_Audio() {}
void CCDAudio::MP3_Suspend_Audio() {}
void CCDAudio::_CDUpdate(int, int) {}
void CCDAudio::_Stop(int, int) {}
void CCDAudio::_PlayFile(const char*, int) {}
static CCDAudio audio;
ICDAudio* cdaudio = &audio;
CCDAudio* GetInteralCDAudio() {
    return &audio;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const WorkerFault fault = static_cast<WorkerFault>(std::atoi(argv[1]));
    CThread worker;
    SetWorkerFault(fault);
    bool initialized = false;
    try {
        initialized = worker.Init();
    } catch (...) {
        std::fputs("Init did not return a controlled failure\n", stderr);
        return 3;
    }
    if (initialized || worker.m_bThreadRunning || worker.m_dwThreadID != 0 || WorkerLiveResources() != 0
        || initializations != (fault == WorkerFault::Thread ? 1 : 0)
        || shutdowns != (fault == WorkerFault::Thread ? 1 : 0)) {
        std::fprintf(stderr, "Failed Init leaked state: running=%d resources=%d init=%d shutdown=%d\n",
            worker.m_bThreadRunning.load() ? 1 : 0, WorkerLiveResources(), initializations, shutdowns);
        return 4;
    }
    const int before = shutdowns;
    if (!worker.Shutdown() || shutdowns != before || WorkerLiveResources() != 0) {
        return 5;
    }
    SetWorkerFault(WorkerFault::None);
    if (!worker.Init() || !worker.Init() || !worker.Shutdown() || !worker.Shutdown()
        || worker.m_bThreadRunning || shutdowns != before + 1 || WorkerLiveResources() != 0) {
        return 6;
    }
    std::puts("Actual worker Init rollback, retry and repeated Shutdown passed");
    return 0;
}
