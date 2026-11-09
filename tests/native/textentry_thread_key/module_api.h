#ifndef CSE_TEST_TEXTENTRY_KEY_MODULE_API_H
#define CSE_TEST_TEXTENTRY_KEY_MODULE_API_H

#include "TextEntryClipboard.h"
#include "os_api.h"

struct KeyFrameModuleApi {
    void* (*begin)(const void*, int);
    bool (*entered)(void*);
    void (*end)(void*);
    const void* (*identity)();
    cse_textentry_clipboard::NativeFrameRetireResult (*retire)(
        const cse_textentry_clipboard::NativeFrameQuiescence&);
    bool (*readBuffer)(cse_textentry_clipboard::Buffer::Reader);
    void (*setNotice)(void (*)(void*, int), void*);
    void (*setFault)(KeyFault);
    void (*setZeroKey)(bool);
    KeyOsStats (*stats)();
};

#endif
