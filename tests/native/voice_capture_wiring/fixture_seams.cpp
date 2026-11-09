#include "fixture_seams.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#if defined(CSE_WIRING_CALLER_OBJECT)
WiringCvar voice_enable{1};
WiringCvar voice_dsound{1};
WiringCvar voice_forcemicrecord{1};
bool g_bVoiceAtLeastPartiallyInitted = false;
bool g_bUsingSteamVoice = true;
IVoiceRecord* g_pVoiceRecord = nullptr;
IVoiceCodec* g_pEncodeCodec = nullptr;
IMixerControls* g_pMixerControls = nullptr;
CSysModule* g_hVoiceCodecDLL = nullptr;
CVoiceChannel g_VoiceChannels[VOICE_NUM_CHANNELS] = {};
#include "actual_sample_format.inc"

namespace fixture {
void SetRate(int rate) {
    g_VoiceSampleFormat.nSamplesPerSec = rate;
}
int DefaultRate() {
    return VOICE_SAMPLE_RATE;
}
}
#include "actual_helpers.inc"
#include "actual_deinit.inc"
#include "actual_init.inc"
#else

namespace fixture {
State state;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "voice wiring: %s\n", message);
        std::exit(1);
    }
}
void CheckClean() {
    Check(!g_bVoiceAtLeastPartiallyInitted && g_pVoiceRecord == nullptr && g_pMixerControls == nullptr
            && g_hVoiceCodecDLL == nullptr && g_pEncodeCodec == nullptr, "actual caller globals are cleared");
    Check(state.captureCreated == state.captureReleased && state.mixerCreated == state.mixerReleased
            && state.codecsCreated == state.codecsReleased && state.moduleLoads == state.moduleUnloads
            && !state.ownerVisibleDuringRelease, "actual deinit releases all acquired objects without stale ownership");
    for (const auto& channel : g_VoiceChannels) {
        Check(channel.m_pVoiceCodec == nullptr, "actual partial channel cleanup cleared every codec");
    }
}
void Reset() {
    Voice_Deinit();
    CheckClean();
    state = {};
    voice_enable.value = 1;
    voice_dsound.value = 1;
    voice_forcemicrecord.value = 1;
    SetRate(DefaultRate());
}

namespace {
class Recorder final : public IVoiceRecord {
public:
    void Release() override {
        state.ownerVisibleDuringRelease = state.ownerVisibleDuringRelease || g_pVoiceRecord == this;
        ++state.captureReleased;
        state.events.push_back("capture-release");
        delete this;
    }
    bool RecordStart() override {
        std::abort();
    }
    void RecordStop() override {
        state.events.push_back("capture-stop");
    }
    void Idle() override {
        std::abort();
    }
    int GetRecordedData(short*, int) override {
        std::abort();
    }
};
class Codec final : public IVoiceCodec {
public:
    explicit Codec(unsigned int ordinal) : ordinal_(ordinal) {
        ++state.codecsCreated;
    }
    bool Init(int quality) override {
        state.qualities.push_back(quality);
        return ordinal_ != state.failedInit;
    }
    void Release() override {
        ++state.codecsReleased;
        state.events.push_back("codec-release");
        delete this;
    }
    int Compress(const char*, int, char*, int, bool) override {
        std::abort();
    }
    int Decompress(const char*, int, char*, int) override {
        std::abort();
    }
    bool ResetState() override {
        std::abort();
    }
private:
    unsigned int ordinal_;
};
class Mixer final : public IMixerControls {
public:
    void Release() override {
        state.ownerVisibleDuringRelease = state.ownerVisibleDuringRelease || g_pMixerControls == this;
        ++state.mixerReleased;
        state.events.push_back("mixer-release");
        delete this;
    }
    bool GetValue_Float(Control, float&) override {
        std::abort();
    }
    bool SetValue_Float(Control, float) override {
        std::abort();
    }
    bool SelectMicrophoneForWaveInput() override {
        ++state.selections;
        return state.selection;
    }
};
void* CodecFactory(const char* name, int*) {
    Check(name != nullptr && *name != 0, "codec receives actual nonempty name");
    ++state.codecRequests;
    if (state.codecRequests == state.missingCodec) {
        return nullptr;
    }
    return new Codec(state.codecRequests);
}
int moduleToken;
int steamToken;
}
}

bool VoiceSE_Init() {
    ++fixture::state.voiceSEStarts;
    return fixture::state.voiceSE;
}
void VoiceSE_Term() {
    ++fixture::state.voiceSETerms;
    fixture::state.events.push_back("voice-se-term");
}
void Voice_EndAllChannels() {
    fixture::state.events.push_back("end-channels-seam");
}
qboolean Voice_RecordStop() {
    ++fixture::state.stopCalls;
    if (!g_bUsingSteamVoice && g_pVoiceRecord != nullptr) {
        g_pVoiceRecord->RecordStop();
    }
    return TRUE;
}
ISteamUser* SteamUser() {
    ++fixture::state.steamQueries;
    return fixture::state.steam ? reinterpret_cast<ISteamUser*>(&fixture::steamToken) : nullptr;
}
void Con_Printf(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    fixture::state.messages.emplace_back(message);
}
void FS_GetLocalCopy(const char* filename) {
    fixture::state.filenames.emplace_back(filename);
}
CSysModule* Sys_LoadModule(const char* filename) {
    fixture::Check(!fixture::state.filenames.empty() && fixture::state.filenames.back() == filename,
        "actual caller fetch/load uses the same native codec filename");
    if (!fixture::state.module) {
        return nullptr;
    }
    ++fixture::state.moduleLoads;
    return reinterpret_cast<CSysModule*>(&fixture::moduleToken);
}
CreateInterfaceFn Sys_GetFactory(CSysModule*) {
    return fixture::state.factory ? fixture::CodecFactory : nullptr;
}
void Sys_UnloadModule(CSysModule* module) {
    fixture::Check(module == reinterpret_cast<CSysModule*>(&fixture::moduleToken)
            && fixture::state.codecsCreated == fixture::state.codecsReleased,
        "actual deinit releases codec objects before unloading its owned codec module");
    ++fixture::state.moduleUnloads;
    fixture::state.events.push_back("module-unload");
}
IMixerControls* GetMixerControls() {
    if (!fixture::state.mixer) {
        return nullptr;
    }
    ++fixture::state.mixerCreated;
    return new fixture::Mixer;
}
namespace cse {
IVoiceRecord* CreateVoiceRecordSDL(int rate, VoiceCapturePolicy policy) {
    fixture::state.attempts.push_back({rate, policy});
    const bool allowed = policy == VoiceCapturePolicy::DirectSound ? fixture::state.allowDS : fixture::state.allowWave;
    if (!allowed) {
        return nullptr;
    }
    ++fixture::state.captureCreated;
    return new fixture::Recorder;
}
}
#if INTPTR_MAX <= INT32_MAX
// Caller-only Win32 reference: native delegates are not selected by production.
IVoiceRecord* CreateVoiceRecord_DSound(int rate) {
    return cse::CreateVoiceRecordSDL(rate, cse::VoiceCapturePolicy::DirectSound);
}
IVoiceRecord* CreateVoiceRecord_WaveIn(int rate) {
    return cse::CreateVoiceRecordSDL(rate, cse::VoiceCapturePolicy::WaveIn);
}
#endif
#endif
