#include "module_api.h"

namespace clipboard = cse_textentry_clipboard;
namespace {
void (*Notice)(void*, int) = nullptr;
void* NoticeContext = nullptr;

void* Begin(const void* panel, int remainder) {
    return new clipboard::PasteFrame(panel, remainder);
}

bool Entered(void* frame) {
    return static_cast<clipboard::PasteFrame*>(frame)->Entered();
}

void End(void* frame) {
    delete static_cast<clipboard::PasteFrame*>(frame);
}

bool Read(clipboard::Buffer::Reader reader) {
    clipboard::Buffer buffer;
    return buffer.Read(4, true, reader) && buffer.Length() == 1 && buffer.Data()[0] == L'Q';
}

void SetNotice(void (*notice)(void*, int), void* context) {
    Notice = notice;
    NoticeContext = context;
}

__attribute__((destructor)) void Unloaded() {
    if (Notice) {
        Notice(NoticeContext, CSE_KEY_MODULE_TAG);
    }
}
}

// Actual compiler-initialized ordinary function-pointer fields. dlsym obtains
// this data object; no C++ member pointer is converted to a raw address.
extern "C" __attribute__((visibility("default"))) const KeyFrameModuleApi CSE_KEY_MODULE_API = {
    Begin, Entered, End, clipboard::NativePasteFrameIdentity, clipboard::RetireNativePasteFrames,
    Read, SetNotice, SetKeyFault, SetZeroKeyMode, ReadKeyOsStats
};
