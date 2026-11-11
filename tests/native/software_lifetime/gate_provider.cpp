#include "engine/native_software_lifetime.h"
namespace cse::software_lifetime {
static bool resourcesReady = false;
bool RetireResources(Mode) {
    return resourcesReady;
}
} // namespace cse::software_lifetime
extern "C" __attribute__((visibility("default"))) void SetResourcesReady(bool ready) {
    cse::software_lifetime::resourcesReady = ready;
}
extern "C" __attribute__((visibility("default"))) bool EnterClient() {
    return cse::software_lifetime::Enter(cse::software_lifetime::Mode::Client);
}
extern "C" __attribute__((visibility("default"))) void LeaveClient() {
    cse::software_lifetime::Leave();
}

extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) {
    return nullptr;
}

#if defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1
#include "common/TextEntryClipboard.h"
static cse_textentry_clipboard::PasteFrame* retainedFrame = nullptr;
extern "C" __attribute__((visibility("default"))) bool StartFrame() {
    retainedFrame = new cse_textentry_clipboard::PasteFrame(&retainedFrame, 1);
    return retainedFrame->Entered();
}
extern "C" __attribute__((visibility("default"))) void EndFrame() {
    delete retainedFrame;
    retainedFrame = nullptr;
}
extern "C" __attribute__((visibility("default"))) bool LateFrameAccepted() {
    cse_textentry_clipboard::PasteFrame frame(&retainedFrame, 1);
    return frame.Entered();
}
#endif
