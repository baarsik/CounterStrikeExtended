#ifndef CSE_CLIPBOARD_MODULE_FIXTURE_H
#define CSE_CLIPBOARD_MODULE_FIXTURE_H
#include <cstdint>

struct FixtureHost {
    void* context;
    std::uint64_t epoch;
    bool (*pin)(void*, std::uint64_t*);
    bool (*defer)(void*, std::uint64_t);
    bool (*retireAllowed)(void*);
    void (*unloaded)(void*, int);
    void (*named)(void*, const unsigned char*, std::uint64_t);
};

extern "C" {
int FixtureTag();
int FixtureCreate(const FixtureHost*, void**);
int FixturePublish(void*);
int FixtureBorrow(void*);
int FixtureReadCell(void*);
int FixtureEnd(void*);
int FixtureRetry(void*);
int FixtureCloseDestroy(void*);
}
#endif
