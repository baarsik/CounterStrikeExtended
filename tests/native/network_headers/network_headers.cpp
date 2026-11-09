#include <cstddef>
#include <type_traits>
#if defined(_WIN32)
#include <Windows.h>
#endif

#if defined(CSE_API_FIRST)
#include "engine_apiproxy.h"
#include "net.h"
#else
#include "net.h"
#include "engine_apiproxy.h"
#endif

static_assert(NS_CLIENT == 0 && NS_SERVER == 1 && NS_MULTICAST == 2,
    "Network source identifiers retain their protocol values");
static_assert(sizeof(netsrc_t) == sizeof(int), "Network source identifiers retain int width");
static_assert(sizeof(netadr_t) == 20 && offsetof(netadr_t, port) == 18,
    "Connectionless packet address compatibility is independent of pointer width");
static_assert(TRUE == 1 && FALSE == 0, "Legacy qboolean constants retain their integer encoding");
static_assert(std::is_same<decltype(TRUE), int>::value && std::is_same<decltype(FALSE), int>::value,
    "Legacy qboolean constants do not become a different overload or template type");

constexpr int kArrayValues[] = {NS_CLIENT, NS_SERVER, NS_MULTICAST};
static_assert(ARRAYSIZE(kArrayValues) == NS_MULTICAST + 1, "Native arrays retain their compile-time element count");

static_assert(sizeof(tagPOINT) == 8 && offsetof(tagPOINT, x) == 0 && offsetof(tagPOINT, y) == 4,
    "The mouse callback retains two signed 32-bit coordinates");
static_assert(std::is_signed<decltype(tagPOINT::x)>::value && std::is_signed<decltype(tagPOINT::y)>::value,
    "Mouse coordinates retain negative screen and window positions");
static_assert(std::is_same<decltype(cl_enginefunc_t::pfnGetMousePos), void (*)(tagPOINT*)>::value,
    "Mouse callback records retain the same tagged point pointer");
