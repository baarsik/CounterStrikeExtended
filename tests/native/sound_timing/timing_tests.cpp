#include <cstdio>
#include <cstdlib>

namespace {
struct DmaState {
    int samples;
    int channels;
    int dmaspeed;
};
struct MixAhead {
    float value;
};
DmaState* shm = nullptr;
MixAhead _snd_mixahead{0.25f};
int paintedtime = 0;
int sound_started = 0;
int snd_blocked = 0;
int position = 0;
int positionCalls = 0;
int submitCalls = 0;
int paintCalls = 0;
int paintedEnd = 0;
int stopCalls = 0;

int SNDDMA_GetDMAPos() {
    ++positionCalls;
    return position;
}

void SNDDMA_Submit() {
    ++submitCalls;
}

void S_StopAllSounds(int clear) {
    if (clear != 1) {
        std::abort();
    }
    ++stopCalls;
}

void S_PaintChannels(int end) {
    ++paintCalls;
    paintedEnd = end;
}

#define CSE_SOUND_USE_SDL 1
#include "actual_timing.inc"

void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}

void Reset(DmaState* state) {
    shm = state;
    paintedtime = 0;
    sound_started = 1;
    snd_blocked = 0;
    soundtime = 0;
    buffers = 0;
    oldsamplepos = 0;
    position = 0;
    positionCalls = 0;
    submitCalls = 0;
    paintCalls = 0;
    stopCalls = 0;
}
}

int main() {
    DmaState stereo{4096, 2, 8000};
    Reset(&stereo);
    position = 1024;
    S_Update_();
    Check(soundtime == 512 && paintedEnd == 1256, "Stereo sample-pair timing changed");
    Check(positionCalls == 1 && submitCalls == 1 && paintCalls == 1, "Active update call order/count differs");
    _snd_mixahead.value = 1.0f;
    S_Update_();
    Check(paintedEnd == 1280, "Mix-ahead exceeds one buffer");
    position = 0;
    S_Update_();
    Check(buffers == 1 && soundtime == 2048 && paintedEnd == 2048, "DMA wrap accounting changed");
    paintedtime = SOUND_TIME_RESET_LIMIT + 1;
    oldsamplepos = 1;
    S_Update_();
    Check(buffers == 0 && paintedtime == 2048 && stopCalls == 1 && soundtime == 0,
        "Large sound-time reset changed");
    DmaState mono{4096, 1, 8000};
    Reset(&mono);
    _snd_mixahead.value = 0.25f;
    position = 1024;
    S_Update_();
    Check(soundtime == 1024 && paintedEnd == 1512, "Mono sample timing changed");
    Reset(&stereo);
    sound_started = 0;
    S_Update_();
    Check(positionCalls == 0 && submitCalls == 0 && paintCalls == 0, "Stopped output was submitted");
    sound_started = 1;
    snd_blocked = 1;
    S_Update_();
    Check(positionCalls == 0 && submitCalls == 0 && paintCalls == 0, "Blocked output was submitted");
#if !defined(_WIN32)
    Reset(nullptr);
    soundtime = 37;
    oldsamplepos = 41;
    paintedtime = SOUND_TIME_RESET_LIMIT + 1;
    GetSoundtime();
    S_Update_();
    Check(positionCalls == 0 && submitCalls == 0 && paintCalls == 0 && stopCalls == 0,
        "Missing DMA storage entered a backend or painter");
    Check(soundtime == 37 && oldsamplepos == 41 && paintedtime == SOUND_TIME_RESET_LIMIT + 1,
        "Missing DMA storage changed timing state");
#endif
    std::puts("PASS sample-pair timing, wrap/reset, mix-ahead and absent-output lifecycle");
}
