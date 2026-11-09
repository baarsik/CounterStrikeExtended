#ifndef CSE_TEST_FRAME_MODULE_API_H
#define CSE_TEST_FRAME_MODULE_API_H

struct FrameModuleApi {
    void* (*begin)(const void* panel, int logicalRemainder);
    bool (*entered)(void* frame);
    void (*end)(void* frame);
};

#endif
