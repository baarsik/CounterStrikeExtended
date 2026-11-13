#include "../../src/engine/thread.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

// The real queue is tested with controlled controller callbacks. The first stop
// holds its worker so subsequent requests are guaranteed to be queued together.
static std::mutex gate;
static std::condition_variable changed;
static bool entered = false;
static bool released = false;
static int stops = 0;
static std::atomic<int> shutdowns{0};
static std::vector<std::string> delivered;

CCDAudio::CCDAudio() {}
CCDAudio::~CCDAudio() {}
int CCDAudio::Init() { return 1; }
void CCDAudio::Shutdown() { ++shutdowns; }
void CCDAudio::Play(int, bool) {}
void CCDAudio::PlayFile(const char *, bool) {}
void CCDAudio::Pause() {}
void CCDAudio::Resume() {}
void CCDAudio::Frame() {}
void CCDAudio::MP3_Resume_Audio() {}
void CCDAudio::MP3_Suspend_Audio() {}
void CCDAudio::_CDUpdate(int, int) {}
void CCDAudio::_Stop(int, int) {
    std::unique_lock<std::mutex> lock(gate);
    if (++stops == 1) {
        entered = true;
        changed.notify_all();
        changed.wait(lock, [] { return released; });
    }
    delivered.emplace_back("stop");
    changed.notify_all();
}
void CCDAudio::_PlayFile(const char *filename, int looping) {
    std::lock_guard<std::mutex> lock(gate);
    delivered.emplace_back(std::string(filename) + (looping ? ":loop" : ":once"));
    changed.notify_all();
}

static CCDAudio audio;
ICDAudio *cdaudio = &audio;
CCDAudio *GetInteralCDAudio() { return &audio; }

static bool WaitForStop() {
    std::unique_lock<std::mutex> lock(gate);
    return changed.wait_for(lock, std::chrono::seconds(5), [] { return entered; });
}

static void Release() {
    std::lock_guard<std::mutex> lock(gate);
    released = true;
    changed.notify_all();
}

int main() {
    // Reused storage must not inherit queued-command counts from prior bytes.
    constexpr unsigned char constructorPoison = 0xa5;
    alignas(CThread) std::array<unsigned char, sizeof(CThread)> storage;
    storage.fill(constructorPoison);
    std::unique_ptr<CThread, void (*)(CThread *)> owner(new (storage.data()) CThread(), [](CThread *value) {
        value->~CThread();
    });
    CThread &worker = *owner;
    if (!worker.Init() || !worker.AddThreadItem(&CCDAudio::_Stop, 0, 0) || !WaitForStop()) {
        return EXIT_FAILURE;
    }
    char filename[] = "media/first.mp3";
    bool queued = worker.AddFileThreadItem(&CCDAudio::_PlayFile, filename, 1);
    std::memset(filename, 'x', sizeof(filename) - 1);
    queued = worker.AddThreadItem(&CCDAudio::_Stop, 0, 0) && queued;
    queued = worker.AddFileThreadItem(&CCDAudio::_PlayFile, "media/second.mp3", 0) && queued;
    queued = worker.AddThreadItem(&CCDAudio::_Stop, 0, 0) && queued;
    Release();
    {
        std::unique_lock<std::mutex> lock(gate);
        const bool complete = changed.wait_for(lock, std::chrono::seconds(5), [] { return delivered.size() == 5; });
        const std::vector<std::string> expected{"stop", "media/first.mp3:loop", "stop", "media/second.mp3:once", "stop"};
        if (!queued || !complete || delivered != expected) {
            std::fprintf(stderr, "Queued file ownership/control order failed.\n");
            return EXIT_FAILURE;
        }
    }
#if defined(_WIN32)
    const HANDLE handle = worker.m_hThread;
#endif
    if (!worker.Shutdown()) {
        return EXIT_FAILURE;
    }
#if defined(_WIN32)
    SetLastError(ERROR_SUCCESS);
    if (WaitForSingleObject(handle, 0) != WAIT_FAILED || GetLastError() != ERROR_INVALID_HANDLE) {
        return EXIT_FAILURE;
    }
#endif
    if (!worker.Shutdown() || shutdowns != 1) {
        return EXIT_FAILURE;
    }
    {
        std::lock_guard<std::mutex> lock(gate);
        entered = false;
        released = false;
        stops = 0;
        delivered.clear();
    }
    if (!worker.Init() || !worker.AddThreadItem(&CCDAudio::_Stop, 0, 0) || !WaitForStop()) {
        return EXIT_FAILURE;
    }
    std::thread shutdown([&worker] { worker.Shutdown(); });
    // Exceed the old 99 ms poll timeout while the command is still in flight.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const bool premature = shutdowns != 1;
    Release();
    shutdown.join();
    if (premature || shutdowns != 2) {
        std::fprintf(stderr, "Audio resources were released before the command finished.\n");
        return EXIT_FAILURE;
    }
    std::puts("Owned file requests, ordered stop/play and in-flight worker shutdown passed.");
    return EXIT_SUCCESS;
}
