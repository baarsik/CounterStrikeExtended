#include "../../GoldSrc-SDK/engine/thread.h"
#include <cstdio>
#include <cstdlib>

class TestAudio final : public ICDAudio {
public:
    int Init() override { return 1; }
    void Shutdown() override { ++shutdowns; }
    void Play(int, bool) override {}
    void PlayFile(const char *, bool) override {}
    void Pause() override {}
    void Resume() override {}
    void Frame() override {}
    void MP3_Resume_Audio() override {}
    void MP3_Suspend_Audio() override {}
    int shutdowns = 0;
};

static TestAudio audio;
ICDAudio *cdaudio = &audio;
CCDAudio *GetInteralCDAudio() { return nullptr; }

int main() {
    CThread worker;
    if (!worker.Init()) {
        return EXIT_FAILURE;
    }
    const HANDLE handle = worker.m_hThread;
    if (!worker.Shutdown()) {
        return EXIT_FAILURE;
    }
    // A joined-and-closed worker cannot retain a usable native thread handle.
    SetLastError(ERROR_SUCCESS);
    const DWORD result = WaitForSingleObject(handle, 0);
    if (result != WAIT_FAILED || GetLastError() != ERROR_INVALID_HANDLE || audio.shutdowns != 1) {
        std::fprintf(stderr, "CD worker retained its thread handle after shutdown.\n");
        return EXIT_FAILURE;
    }
    if (!worker.Shutdown() || audio.shutdowns != 1) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
