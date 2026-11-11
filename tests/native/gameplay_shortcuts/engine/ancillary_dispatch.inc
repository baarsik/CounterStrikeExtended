// Audio update and playback dispatch bodies are exact extracted production code.
cvar_t snd_noextraupdate{};
int audioUpdates = 0;
void S_Update_() {
    ++audioUpdates;
}
#include "extra_update.inc"
bool PlaybackKey(const SDL_Event &) {
    return false;
}
void Sys_Sleep(int milliseconds) {
    CHECK(milliseconds == 1);
}
void WebmDispatch() {
#include "webm_dispatch.inc"
    CHECK(!skipped);
}
