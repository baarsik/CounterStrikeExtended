#include "frame_module_api.h"
#include "TextEntryClipboard.h"

static void* BeginFrame(const void* panel, int remainder) {
    return new cse_textentry_clipboard::PasteFrame(panel, remainder);
}

static bool FrameEntered(void* frame) {
    return static_cast<cse_textentry_clipboard::PasteFrame*>(frame)->Entered();
}

static void EndFrame(void* frame) {
    delete static_cast<cse_textentry_clipboard::PasteFrame*>(frame);
}

#ifndef CSE_FRAME_API_NAME
#error Each fixture image must have a distinct public data symbol
#endif

extern "C" __attribute__((visibility("default"))) const FrameModuleApi CSE_FRAME_API_NAME = {
    BeginFrame, FrameEntered, EndFrame
};
