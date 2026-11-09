#include "pr_dlls.h"

namespace {
#if defined(CSE_FIRST_MODULE)
constexpr int kModuleValue = 100;
#else
constexpr int kModuleValue = 200;
#endif
int entityCalls = 0;
}

extern "C" {
void Dispatch(entvars_s*, void* context) {
    *static_cast<int*>(context) += kModuleValue;
}
void Entity(entvars_s*) {
    ++entityCalls;
}
#if !defined(CSE_FIRST_MODULE)
void OnlySecond(entvars_s*, void* context) {
    *static_cast<int*>(context) += kModuleValue;
}
#endif
int EntityCalls() {
    return entityCalls;
}
int ModuleValue() {
    return kModuleValue;
}
}
