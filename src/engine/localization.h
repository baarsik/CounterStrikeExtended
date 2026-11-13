#ifndef CSE_ENGINE_LOCALIZATION_H
#define CSE_ENGINE_LOCALIZATION_H

#include "interface.h"

namespace cse {
namespace localization {
void Initialize();
void InitializeCommand();
void Shutdown();
void BindRuntime(CreateInterfaceFn factory);
void UnbindRuntime();
void ProcessFrame();
const char* Language();
} // namespace localization
} // namespace cse

#endif
