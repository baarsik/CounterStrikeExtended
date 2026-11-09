#ifndef CSE_SDL_BRIDGE_FIXTURE_API_H
#define CSE_SDL_BRIDGE_FIXTURE_API_H

#include <stdint.h>

#if defined(_WIN32)
#if defined(CSE_BRIDGE_FIXTURE_BUILD)
#define CSE_BRIDGE_FIXTURE_API __declspec(dllexport)
#else
#define CSE_BRIDGE_FIXTURE_API __declspec(dllimport)
#endif
#else
#define CSE_BRIDGE_FIXTURE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum FixtureCounter {
    FixtureMainCheck,
    FixtureInitCheck,
    FixtureDeviceCheck,
    FixtureVideoCheck,
    FixtureMouseCheck,
    FixtureModeCheck,
    FixtureDefaultCheck,
    FixturePublicCheck,
    FixtureThreadCheck,
    FixtureHandlerCalled,
    FixtureSetterCheck,
    FixtureCurrentHandlerCheck,
    FixtureUnexpectedEffect,
    FixtureCounterCount
};

CSE_BRIDGE_FIXTURE_API void FixtureReset(void);
CSE_BRIDGE_FIXTURE_API void FixtureSetForeign(uint32_t foreign);
CSE_BRIDGE_FIXTURE_API void FixtureSetRelative(uint32_t relative);
CSE_BRIDGE_FIXTURE_API void FixtureSetReady(uint32_t ready);
CSE_BRIDGE_FIXTURE_API void FixtureSetThread(uint32_t mainThread, uint32_t videoThread);
CSE_BRIDGE_FIXTURE_API void FixtureSetDevice(uint32_t present, uint32_t quitting, uint32_t poison);
CSE_BRIDGE_FIXTURE_API void FixtureSetCustomHandler(uint32_t custom);
CSE_BRIDGE_FIXTURE_API uint32_t FixtureCount(uint32_t counter);
CSE_BRIDGE_FIXTURE_API uint64_t FixtureSnapshot(void);
CSE_BRIDGE_FIXTURE_API uint32_t FixtureNaiveQuery(void);

#ifdef __cplusplus
}
#endif
#endif
