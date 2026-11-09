#ifndef CSE_VOICE_CAPTURE_WIRING_SEAMS_H
#define CSE_VOICE_CAPTURE_WIRING_SEAMS_H
#include "ivoicecodec.h"
#include "ivoicerecord.h"
#include "voice_mixer_controls.h"
#include "voice_record_sdl.h"
#include "const.h"
#include <cstdint>
#include <string>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#else
constexpr qboolean FALSE = 0;
constexpr qboolean TRUE = 1;
#endif
#include "actual_constants.inc"

struct WiringCvar {
    float value;
};
struct CVoiceChannel {
    IVoiceCodec* m_pVoiceCodec;
};
class ISteamUser;
extern WiringCvar voice_enable;
extern WiringCvar voice_dsound;
extern WiringCvar voice_forcemicrecord;
extern bool g_bVoiceAtLeastPartiallyInitted;
extern bool g_bUsingSteamVoice;
extern IVoiceRecord* g_pVoiceRecord;
extern IVoiceCodec* g_pEncodeCodec;
extern IMixerControls* g_pMixerControls;
extern CSysModule* g_hVoiceCodecDLL;
extern CVoiceChannel g_VoiceChannels[VOICE_NUM_CHANNELS];
qboolean Voice_Init(const char* codec, int quality);
void Voice_Deinit();
void Voice_ReleaseCaptureForModuleUnload();
void Voice_ReleaseMixerForModuleUnload();
bool VoiceSE_Init();
void VoiceSE_Term();
void Voice_EndAllChannels();
qboolean Voice_RecordStop();
ISteamUser* SteamUser();
void Con_Printf(const char* format, ...);
void FS_GetLocalCopy(const char* filename);

namespace fixture {
struct Attempt {
    int sampleRate;
    cse::VoiceCapturePolicy policy;
};
struct State {
    bool allowDS = true;
    bool allowWave = true;
    bool steam = false;
    bool voiceSE = true;
    bool module = true;
    bool factory = true;
    bool mixer = true;
    bool selection = true;
    unsigned int missingCodec = 0;
    unsigned int failedInit = 0;
    unsigned int captureCreated = 0;
    unsigned int captureReleased = 0;
    unsigned int mixerCreated = 0;
    unsigned int mixerReleased = 0;
    unsigned int codecsCreated = 0;
    unsigned int codecsReleased = 0;
    unsigned int codecRequests = 0;
    unsigned int selections = 0;
    unsigned int moduleLoads = 0;
    unsigned int moduleUnloads = 0;
    unsigned int steamQueries = 0;
    unsigned int voiceSEStarts = 0;
    unsigned int voiceSETerms = 0;
    unsigned int stopCalls = 0;
    bool ownerVisibleDuringRelease = false;
    std::vector<Attempt> attempts;
    std::vector<std::string> messages;
    std::vector<std::string> filenames;
    std::vector<std::string> events;
    std::vector<int> qualities;
};
extern State state;
void Reset();
void SetRate(int rate);
int DefaultRate();
void Check(bool condition, const char* message);
void CheckClean();
}
#endif
