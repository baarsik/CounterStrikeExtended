#include "cd.h"
#include "thread.h"
#include "../../../src/common/sdl3_policy.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {
constexpr int BarrierLimitSeconds = 5;
constexpr int TestTrack = 2;
constexpr int TrackCount = 29;
constexpr int MaximumTracks = 200;
constexpr int InitialPosition = 1234;
constexpr int FocusRegressionFailure = 1;
std::atomic<int> barrier{0};
int issuedBarrier = 0;

void Check(bool pass, const char *message) {
    if (!pass) {
        std::fprintf(stderr, "FAIL music focus: %s\n", message);
        std::_Exit(FocusRegressionFailure);
    }
}
struct BackendSnapshot {
    bool playing;
    int position;
    int opens;
    int pauses;
};
struct Backend {
    std::mutex mutex;
    bool active = false;
    bool paused = false;
    bool failInit = false;
    bool failOpen = false;
    bool failStart = false;
    bool failPause = false;
    bool failResume = false;
    std::atomic<bool> blockPause{false};
    std::atomic<bool> pauseEntered{false};
    int position = 0;
    int opens = 0;
    int pauses = 0;
    bool Init() {
        return !failInit;
    }
    bool Open(const char *, bool) {
        std::lock_guard<std::mutex> lock(mutex);
        ++opens;
        active = false;
        paused = false;
        position = 0;
        return !failOpen;
    }
    bool Start() {
        std::lock_guard<std::mutex> lock(mutex);
        active = !failStart;
        return active;
    }
    void Pause(bool value) {
        std::lock_guard<std::mutex> lock(mutex);
        ++pauses;
        pauseEntered.store(true);
        while (blockPause.load()) {
            std::this_thread::yield();
        }
        if (active && !(value ? failPause : failResume)) {
            paused = value;
        }
    }
    bool IsPlaying() {
        std::lock_guard<std::mutex> lock(mutex);
        return active && !paused;
    }
    void Stop() {
        std::lock_guard<std::mutex> lock(mutex);
        active = false;
    }
    void Shutdown() {
        Stop();
    }
    void SetVolume(float) {
    }
    BackendSnapshot Snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return {active && !paused, position, opens, pauses};
    }
} musicPlayer;
} // namespace

double realtime = 100.0;
struct Cvar {
    float value;
};
Cvar bgmvolume{1.0f};
Cvar MP3Volume{1.0f};
Cvar MP3FadeTime{0.5f};
constexpr int MAX_MP3_TRACKS = MaximumTracks;
constexpr int NUM_BUILTIN_MP3_TRACKS = TrackCount;
char *g_pszMP3trackFileMap[MaximumTracks]{};
int g_iMP3NumTracks = 0;
int g_iMP3FirstMalloc = 0;
const char com_empty_string[] = "";
static CCDAudio g_CDAudio;
ICDAudio *cdaudio = &g_CDAudio;
CCDAudio *GetInteralCDAudio() {
    return &g_CDAudio;
}
void SetStreamVolumePan(float value) {
    musicPlayer.SetVolume(value);
}
int COM_CheckParm(const char *) {
    return 0;
}
int MP3_GetTrack(const char *) {
    return TestTrack;
}
bool FS_GetLocalPath(const char *name, char *out, int bytes) {
    return std::snprintf(out, static_cast<std::size_t>(bytes), "%s", name) < bytes;
}
void FS_GetLocalCopy(const char *) {
}
void Q_strncpy(char *out, const char *in, std::size_t count) {
    std::snprintf(out, count, "%s", in);
}
void Con_Printf(const char *, ...) {
}
double Sys_FloatTime() {
    return realtime;
}
const char *va(const char *, ...) {
    return "";
}
void Cvar_DirectSet(Cvar *, const char *) {
}
void CCDAudio::FreeAllocatedTracks() {
}
void CCDAudio::_GetAudioDiskInfo(int first, int) {
    if (first != 0) {
        barrier.store(first, std::memory_order_release);
    }
}
#include "actual_audio.inc"

bool host_initialized = true;
cse::FractionalMotion s_wheelMotion;
bool bPausedByCareer = false;
constexpr int src_command = 0;
struct {
    bool paused = false;
} cl;
struct Video {
    bool GetInitialized() const {
        return false;
    }
} video;
Video *videomode = &video;
struct Surface {
    void CalculateMouseVisible() {
    }
    void GetMouseDelta(int *, int *) {
    }
} g_BaseUISurface;
void ClearIOStates() {
}
bool VGuiWrap2_IsGameUIVisible() {
    return false;
}
bool VGuiWrap2_IsInCareerMatch() {
    return false;
}
void ClientDLL_ActivateMouse() {
}
void ClientDLL_DeactivateMouse() {
}
void Cmd_ExecuteString(const char *, int) {
}
class CGame {
  public:
    void AppActivate(bool);
    bool m_bActiveApp = true;
};
#if defined(CSE_MUSIC_HARDWARE)
#include "actual_activate_hardware.inc"
#else
#include "actual_activate_software.inc"
#endif

namespace {
void Drain() {
    const int token = ++issuedBarrier;
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(BarrierLimitSeconds);
    while (!thread->AddThreadItem(&CCDAudio::_GetAudioDiskInfo, token, 0)) {
        Check(std::chrono::steady_clock::now() < limit, "bounded real queue barrier admission");
        std::this_thread::yield();
    }
    while (barrier.load(std::memory_order_acquire) != token) {
        Check(std::chrono::steady_clock::now() < limit, "bounded real worker barrier dispatch");
        std::this_thread::yield();
    }
}
void Start(bool file = false) {
    Check(thread->Init(), "actual worker/controller initialization");
    Drain();
    if (file) {
        g_pszMP3trackFileMap[TestTrack] = const_cast<char *>("authored.mp3");
        g_CDAudio.PlayFile("authored.mp3", true);
    } else {
        g_CDAudio.Play(TestTrack, true);
    }
    Drain();
    Check(g_CDAudio.m_bIsPlaying && musicPlayer.IsPlaying(), "actual controller starts authored test track");
    musicPlayer.position = InitialPosition;
}
void Pause(CGame &game) {
    game.AppActivate(false);
    Drain();
    Check(!g_CDAudio.m_bIsPlaying && !musicPlayer.IsPlaying(), "focus loss pauses current stream");
}
void Gain(CGame &game) {
    game.AppActivate(true);
    Drain();
}
struct QueueHold {
    CThread *worker = static_cast<CThread *>(thread);
    QueueHold() {
        Check(pthread_mutex_lock(&worker->m_mutex) == 0, "hold real recursive command mutex");
    }
    ~QueueHold() {
        Check(pthread_mutex_unlock(&worker->m_mutex) == 0, "release real command mutex");
    }
};
} // namespace
int main(int argc, char **argv) {
    Check(argc == 2, "one named focus case");
    const char *name = argv[1];
    CGame game;
    const bool suspended = std::strncmp(name, "suspend_", 8) == 0;
    Start(std::strcmp(name, "suspend_file_failure") == 0);
    if (std::strcmp(name, "primed_track_zero") == 0) {
        char filename[] = "primed.mp3";
        g_pszMP3trackFileMap[TestTrack] = filename;
        g_CDAudio.PrimeTrack(filename, 1);
        Drain();
        const auto prime = musicPlayer.Snapshot();
        Check(!prime.playing && prime.opens == 2, "priming opens the requested stream but does not start it");
        g_CDAudio.Play(0, true);
        Drain();
        const auto played = musicPlayer.Snapshot();
        Check(played.playing && played.opens == prime.opens, "Play(0) starts the already primed stream");
    } else if (std::strcmp(name, "ordinary") == 0) {
        Check(s_wheelMotion.TakeY(0.75f) == 0, "partial wheel motion before focus loss");
        Pause(game);
        Gain(game);
        Check(s_wheelMotion.TakeY(0.5f) == 0, "fresh wheel gesture starts after focus returns");
        Check(s_wheelMotion.TakeY(0.5f) == 1, "fresh wheel gesture completes a detent");
        const auto state = musicPlayer.Snapshot();
        Check(state.playing && g_CDAudio.m_bIsPlaying, "ordinary focus gain resumes playback");
        Check(state.position == InitialPosition && state.opens == 1, "ordinary resume preserves stream/position");
    } else if (std::strcmp(name, "manual_pause") == 0) {
        g_CDAudio.Pause();
        Drain();
        game.AppActivate(false);
        Gain(game);
        Check(!musicPlayer.IsPlaying() && !g_CDAudio.m_bIsPlaying, "manual pause survives focus changes");
    } else if (std::strcmp(name, "manual_after_loss") == 0) {
        Pause(game);
        g_CDAudio.Pause();
        Drain();
        Gain(game);
        Check(!musicPlayer.IsPlaying(), "manual pause after focus loss cancels automatic resume");
    } else if (std::strcmp(name, "new_play_failure") == 0) {
        Pause(game);
        musicPlayer.failInit = true;
        g_CDAudio.Play(TestTrack, false);
        Drain();
        musicPlayer.failInit = false;
        Gain(game);
        Check(!musicPlayer.IsPlaying(), "failed new Play does not restore the previous focus-paused track");
    } else if (std::strcmp(name, "stopped") == 0) {
        g_CDAudio.Stop();
        Drain();
        game.AppActivate(false);
        Gain(game);
        Check(!musicPlayer.IsPlaying() && !g_CDAudio.m_bIsPlaying, "stopped controller remains stopped");
    } else if (std::strcmp(name, "finished") == 0) {
        musicPlayer.active = false;
        g_CDAudio.Frame();
        Drain();
        game.AppActivate(false);
        Gain(game);
        Check(!musicPlayer.IsPlaying() && !g_CDAudio.m_bIsPlaying && musicPlayer.Snapshot().opens == 1,
              "finished track is not restarted by focus changes");
    } else if (std::strcmp(name, "disabled") == 0) {
        Pause(game);
        g_CDAudio.m_bEnabled = false;
        Gain(game);
        Check(!musicPlayer.IsPlaying() && !g_CDAudio.m_bIsPlaying, "disabled controller acquires no playback");
    } else if (std::strcmp(name, "rapid") == 0 || std::strcmp(name, "manual_interleave") == 0 ||
               std::strcmp(name, "stop_interleave") == 0 || std::strcmp(name, "new_play") == 0) {
        const bool manual = std::strcmp(name, "manual_interleave") == 0;
        const bool stop = std::strcmp(name, "stop_interleave") == 0;
        const bool play = std::strcmp(name, "new_play") == 0;
        {
            QueueHold hold;
            if (std::strcmp(name, "rapid") == 0) {
                constexpr int CompletePairsBeforeLast = MAX_THREAD_CMDS / 2 - 1;
                for (int index = 0; index < CompletePairsBeforeLast; ++index) {
                    game.AppActivate(false);
                    game.AppActivate(true);
                }
            }
            game.AppActivate(false);
            if (manual) {
                g_CDAudio.Pause();
            }
            if (stop) {
                g_CDAudio.Stop();
            }
            if (play) {
                g_CDAudio.Play(TestTrack, false);
            }
            game.AppActivate(true);
        }
        Drain();
        Check(musicPlayer.IsPlaying() == (!manual && !stop), "admitted focus preserves manual command order");
        if (stop) {
            Check(!g_CDAudio.m_MP3.inuse && !g_CDAudio.m_bWasPlaying, "accepted Stop clears all restart intent");
        }
        if (play) {
            Check(!g_CDAudio.m_MP3.looping, "new Play retains its loop intent");
        }
    } else if (std::strcmp(name, "late_loss_after_play") == 0) {
        Pause(game);
        {
            QueueHold hold;
            game.AppActivate(true);
            g_CDAudio.Play(TestTrack, false);
            game.AppActivate(false);
        }
        Drain();
        Check(!musicPlayer.IsPlaying(), "later focus loss pauses a Play accepted before that loss");
        Gain(game);
        Check(musicPlayer.IsPlaying(), "next gain resumes the newly focus-paused track");
    } else if (std::strcmp(name, "overflow_latest_gain") == 0) {
        {
            QueueHold hold;
            constexpr int FocusRequestsBeyondCapacity = MAX_THREAD_CMDS + 3;
            for (int index = 0; index < FocusRequestsBeyondCapacity; ++index) {
                game.AppActivate(index % 2 == 0);
            }
        }
        Drain();
        Check(!musicPlayer.IsPlaying(), "last admitted loss precedes rejected latest gain");
        g_CDAudio.Frame();
        Drain();
        Check(musicPlayer.IsPlaying(), "latest rejected focus state survives older callback dispatch");
        const auto recovered = musicPlayer.Snapshot();
        constexpr int StableFrames = 3;
        for (int index = 0; index < StableFrames; ++index) {
            g_CDAudio.Frame();
            Drain();
        }
        const auto stable = musicPlayer.Snapshot();
        Check(stable.opens == recovered.opens && stable.pauses == recovered.pauses,
              "Frame retries admission once and never restarts playback per frame");
    } else if (std::strcmp(name, "queue_full") == 0) {
        Pause(game);
        {
            QueueHold hold;
            for (int index = 0; index < MAX_THREAD_CMDS; ++index) {
                Check(thread->AddThreadItem(&CCDAudio::_GetAudioDiskInfo, 0, 0), "fill actual bounded queue");
            }
            Check(!thread->AddThreadItem(&CCDAudio::_GetAudioDiskInfo, 0, 0), "actual full queue rejects next command");
            game.AppActivate(true);
        }
        Drain();
        g_CDAudio.Frame();
        Drain();
        Check(musicPlayer.IsPlaying() && g_CDAudio.m_bIsPlaying, "rejected focus gain retries admission");
    } else if (std::strcmp(name, "failed_pause") == 0) {
        musicPlayer.failPause = true;
        game.AppActivate(false);
        Drain();
        Check(musicPlayer.IsPlaying() && g_CDAudio.m_bIsPlaying, "failed device pause retains truthful playback state");
        musicPlayer.failPause = false;
        Pause(game);
        Gain(game);
        Check(musicPlayer.IsPlaying(), "later focus request retries failed pause");
    } else if (std::strcmp(name, "failed_resume") == 0) {
        Pause(game);
        musicPlayer.failResume = true;
        Gain(game);
        Check(!musicPlayer.IsPlaying() && !g_CDAudio.m_bIsPlaying, "failed resume retains paused truth");
        musicPlayer.failResume = false;
        Gain(game);
        Check(musicPlayer.IsPlaying() && musicPlayer.Snapshot().opens == 1,
              "explicit later focus request retries resume");
    } else if (std::strcmp(name, "suspend_manual_pause") == 0) {
        g_CDAudio.MP3_Suspend_Audio();
        g_CDAudio.Pause();
        Drain();
        Gain(game);
        Check(!musicPlayer.IsPlaying(), "manual pause cancels explicit suspended auto-restore");
        g_CDAudio.Resume();
        Drain();
        Check(musicPlayer.IsPlaying(), "explicit manual Resume can restore the paused suspended track");
    } else if (std::strcmp(name, "suspend_new_play_failure") == 0) {
        g_CDAudio.MP3_Suspend_Audio();
        musicPlayer.failInit = true;
        g_CDAudio.Play(TestTrack, false);
        Drain();
        musicPlayer.failInit = false;
        Gain(game);
        Check(!musicPlayer.IsPlaying(), "failed new Play cancels old suspended automatic restoration");
    } else if (std::strcmp(name, "suspend_stop_after_frame") == 0 ||
               std::strcmp(name, "suspend_stop_after_frame_disabled") == 0) {
        g_CDAudio.MP3_Suspend_Audio();
        g_CDAudio.Frame();
        Drain();
        Check(!g_CDAudio.m_bIsPlaying && !g_CDAudio.m_bWasPlaying,
              "periodic observation clears playing without creating ordinary pause state");
        const int beforeStopOpens = musicPlayer.Snapshot().opens;
        const bool disabled = std::strcmp(name, "suspend_stop_after_frame_disabled") == 0;
        if (disabled) {
            g_CDAudio.m_bEnabled = false;
        }
        g_CDAudio.Stop();
        Drain();
        const bool cancelled = !g_CDAudio.m_MP3.suspended && !g_CDAudio.m_MP3.playing;
        if (disabled) {
            g_CDAudio.m_bEnabled = true;
        }
        Gain(game);
        const auto afterGain = musicPlayer.Snapshot();
        std::fprintf(stderr, "suspended_stop_cancelled=%d reopens_after_stop=%d playing_after_gain=%d\n", cancelled,
                     afterGain.opens - beforeStopOpens, afterGain.playing);
        Check(cancelled && afterGain.opens == beforeStopOpens && !afterGain.playing && !g_CDAudio.m_bIsPlaying,
              "accepted Stop cancels suspended restoration after Frame observes completion");
    } else if (suspended) {
        g_CDAudio.m_flPlayTime = 80.0f;
        g_CDAudio.m_dStartTime = 10.0;
        g_CDAudio.m_dPauseTime = 20.0;
        g_CDAudio.m_dFadeOutTime = 30.0;
        g_CDAudio.m_MP3.volume = 0.35f;
        g_CDAudio.MP3_Suspend_Audio();
        const auto saved = g_CDAudio.m_MP3;
        musicPlayer.failInit = std::strcmp(name, "suspend_init_failure") == 0;
        musicPlayer.failOpen = std::strcmp(name, "suspend_open_failure") == 0 ||
                               std::strcmp(name, "suspend_file_failure") == 0 ||
                               std::strcmp(name, "suspend_stop_after_failure") == 0;
        musicPlayer.failStart = std::strcmp(name, "suspend_start_failure") == 0;
        Gain(game);
        Check(!g_CDAudio.m_bIsPlaying && !musicPlayer.IsPlaying() && g_CDAudio.m_MP3.suspended,
              "failed suspended restoration remains stopped/retryable");
        Check(std::strcmp(g_CDAudio.m_MP3.trackname, saved.trackname) == 0 &&
                  g_CDAudio.m_MP3.tracknum == saved.tracknum && g_CDAudio.m_MP3.looping == saved.looping &&
                  g_CDAudio.m_MP3.volume == saved.volume,
              "failed reopen preserves authored retry metadata");
        Check(g_CDAudio.m_flPlayTime == 80.0f && g_CDAudio.m_dStartTime == 10.0 && g_CDAudio.m_dPauseTime == 20.0 &&
                  g_CDAudio.m_dFadeOutTime == 30.0,
              "failed suspended reopen preserves controller clocks");
        const bool stoppedAfterFailure = std::strcmp(name, "suspend_stop_after_failure") == 0;
        if (stoppedAfterFailure) {
            g_CDAudio.Stop();
            Drain();
        }
        musicPlayer.failInit = false;
        musicPlayer.failOpen = false;
        musicPlayer.failStart = false;
        Gain(game);
        if (stoppedAfterFailure) {
            Check(!g_CDAudio.m_bIsPlaying && !musicPlayer.IsPlaying() && !g_CDAudio.m_MP3.suspended,
                  "Stop cancels suspended retry intent");
        } else {
            Check(g_CDAudio.m_bIsPlaying && musicPlayer.IsPlaying() && !g_CDAudio.m_MP3.suspended,
                  "later focus gain restores suspended track");
        }
    } else if (std::strcmp(name, "shutdown_inflight") == 0) {
        musicPlayer.blockPause.store(true);
        game.AppActivate(false);
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(BarrierLimitSeconds);
        while (!musicPlayer.pauseEntered.load()) {
            Check(std::chrono::steady_clock::now() < limit, "worker entered controlled backend pause");
            std::this_thread::yield();
        }
        std::atomic<bool> closed{false};
        bool result = false;
        std::thread closer([&] {
            result = thread->Shutdown();
            closed.store(true);
        });
        auto *worker = static_cast<CThread *>(thread);
        while (worker->m_bThreadRunning.load()) {
            Check(std::chrono::steady_clock::now() < limit, "shutdown rejects work before join");
            std::this_thread::yield();
        }
        Check(!closed.load(), "actual worker join waits for in-flight focus backend call");
        game.AppActivate(true);
        musicPlayer.blockPause.store(false);
        closer.join();
        Check(result && closed.load() && !musicPlayer.IsPlaying(), "in-flight focus completes before owned shutdown");
    } else if (std::strcmp(name, "shutdown") == 0) {
        game.AppActivate(false);
        Check(thread->Shutdown(), "actual worker rejects/discards/joins focus work before controller shutdown");
        game.AppActivate(true);
        Check(!thread->AddThreadItem(&CCDAudio::_GetAudioDiskInfo, 0, 0), "closed queue accepts no callbacks");
        Check(!musicPlayer.IsPlaying(), "joined shutdown leaves backend stopped");
    } else {
        Check(false, "known case name");
    }
    Check(thread->Shutdown(), "actual worker shutdown joins every owner callback");
    std::printf("PASS actual controller/worker focus case %s\n", name);
}
