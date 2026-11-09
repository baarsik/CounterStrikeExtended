#ifndef CSE_TEST_ACTUAL_VGUI2_TIME_TYPE_H
#define CSE_TEST_ACTUAL_VGUI2_TIME_TYPE_H

#if defined(CSE_TIME_ENTRY_engine)
#include "engine/vgui/ISystem.h"
#elif defined(CSE_TIME_ENTRY_standalone)
#include "ISystem.h"
#elif defined(CSE_TIME_ENTRY_cs_wrapper)
#include "cstrike/public/vgui/ISystem.h"
#else
#include "vgui/ISystem.h"
#endif

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

using ActualTimeMillis = decltype(std::declval<vgui2::ISystem&>().GetTimeMillis());
static_assert(sizeof(ActualTimeMillis) == sizeof(std::int32_t), "VGUI2 milliseconds require signed32 storage");
static_assert(std::numeric_limits<ActualTimeMillis>::is_signed, "VGUI2 milliseconds retain signed ordering");
#if defined(_WIN32)
static_assert(std::is_same<ActualTimeMillis, long>::value, "Windows retains its original long signature");
#else
static_assert(std::is_same<ActualTimeMillis, std::int32_t>::value, "Native time boundary is explicitly int32");
#endif

extern "C" ActualTimeMillis Vgui2FakeTimeProducer(double validMilliseconds);

#endif
