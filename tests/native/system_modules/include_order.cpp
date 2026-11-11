#include "IBaseSystem.h"
#include "BaseSystemModule.h"
#include "engine/IBaseSystem.h"
#include "HLTV/Core/private/common/IBaseSystem.h"
#include "HLTV/DemoPlayer/private/common/IBaseSystem.h"
#include "engine/IDemoPlayer.h"
#include "HLTV/DemoPlayer/private/HLTV/common/IDemoPlayer.h"
#include "HLTV/DemoPlayer/private/HLTV/common/IWorld.h"
#include "HLTV/common/IWorld.h"
#include "HLTV/DemoPlayer/private/common/IEngineWrapper.h"
#include "engine/SystemWrapper.h"
#include <type_traits>

static_assert(std::is_convertible<IEngineWrapper*, ISystemModule*>::value, "engine view has no unique module base");
static_assert(std::is_convertible<IDemoPlayer*, ISystemModule*>::value, "demo view has no unique module base");
static_assert(std::is_convertible<IWorld*, ISystemModule*>::value, "world view has no unique module base");
static_assert(std::is_convertible<EngineWrapper*, ISystemModule*>::value, "engine provider has an ambiguous module base");
#ifdef _WIN32
static_assert(std::is_same<decltype(&IBaseSystem::GetPanel), void* (IBaseSystem::*)()>::value,
    "native Windows panel slot must stay opaque and identical across include roots");
#endif
int main() { return 0; }
