#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef unsigned char byte;
#if defined(CSE_ABI_ENGINE_VIEW)
typedef float vec_t;
typedef vec_t vec3_t[3];
#else
#include "../../../GoldSrc-SDK/cl_dll/util_vector.h"
#endif
typedef int (*pfnUserMsgHook)(const char*, int, void*);
#include "../../../GoldSrc-SDK/cl_dll/wrect.h"
#include "../../../GoldSrc-SDK/common/const.h"
#if defined(CSE_ABI_PROTOTYPE_MUTANT) || defined(CSE_ABI_DESTINATION_MUTANT) \
    || defined(CSE_ABI_MOUSE_SOURCE_MUTANT) || defined(CSE_ABI_MOUSE_DESTINATION_MUTANT)
typedef int HANDLE_SPRITE;
#define HSPRITE HANDLE_SPRITE
#if defined(CSE_ABI_PROTOTYPE_MUTANT)
#include "mutant_prototype_APIProxy.h"
#elif defined(CSE_ABI_MOUSE_SOURCE_MUTANT)
#include "mutant_mouse_source_APIProxy.h"
#elif defined(CSE_ABI_MOUSE_DESTINATION_MUTANT)
#include "mutant_mouse_destination_APIProxy.h"
#else
#include "mutant_destination_APIProxy.h"
#endif
#undef HSPRITE
#endif
#include "cdll_int.h"

#if defined(CSE_ABI_ENGINE_VIEW)
#include "engine_model_view.h"
#include "../../../GoldSrc-SDK/common/client_module_api_types.h"
#include "../../../GoldSrc-SDK/engine/pm_defs.h"
#include "../../../GoldSrc-SDK/engine/kbutton.h"
#else
#if defined(CSE_ABI_NESTED_MUTANT)
#include "mutant_event_api.h"
#endif
#include "../../../GoldSrc-SDK/common/client_module_types.h"
#endif
