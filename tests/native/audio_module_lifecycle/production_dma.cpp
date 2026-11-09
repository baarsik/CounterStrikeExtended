#include "../../../GoldSrc-SDK/engine/sdl3_audio_backend.h"
extern cse::SdlAudioBackend audioBackend;
struct FixtureDma {
    bool soundalive;
    unsigned char* buffer;
};
static FixtureDma sn{};
#include "production_dma_close.inc"
