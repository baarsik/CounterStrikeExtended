#ifndef CSE_NATIVE_DEDICATED_LIFETIME_H
#define CSE_NATIVE_DEDICATED_LIFETIME_H

#include <stdint.h>

#define CSE_NATIVE_DEDICATED_LIFETIME_VERSION 1u

enum CseNativeDedicatedRetireResult {
    CSE_DEDICATED_RETIRE_READY = 0,
    CSE_DEDICATED_RETIRE_UNSUPPORTED_VERSION = 1,
    CSE_DEDICATED_RETIRE_SESSION_ACTIVE = 2,
    CSE_DEDICATED_RETIRE_SHUTDOWN_REQUIRED = 3,
    CSE_DEDICATED_RETIRE_ALREADY_RETIRED = 4,
    CSE_DEDICATED_RETIRE_TRANSPORT_FAILURE = 5
};

typedef uint32_t (*CseNativeDedicatedRetireFn)(uint32_t interfaceVersion);

// Native Apple software/dedicated image lifetime extension; no virtual slots.
// Call on the engine/Steam owning thread after completed dedicated API Shutdown
// and before unloading this image, while its SDK provider is still loaded.
// READY or ALREADY_RETIRED permits unload. Every other result forbids unload.
// Successful retirement is terminal: this image's dedicated Init then fails.
// Ordinary API Shutdown alone remains reusable for admin stop/start and maps.
#ifdef __cplusplus
extern "C" {
#endif
uint32_t CseNativeDedicatedRetireV1(uint32_t interfaceVersion);
#ifdef __cplusplus
}
#endif

#endif
