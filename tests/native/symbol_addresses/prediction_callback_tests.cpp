#include <cstdio>

class CBaseEntity;
enum USE_TYPE { USE_OFF, USE_ON };
using BASEPTR = void (CBaseEntity::*)();
using ENTITYFUNCPTR = void (CBaseEntity::*)(CBaseEntity *);
using USEPTR = void (CBaseEntity::*)(CBaseEntity *, CBaseEntity *, USE_TYPE, float);

class Primary {
public:
    virtual ~Primary() = default;
    int padding = 0;
};

class Secondary {
public:
    int calls = 0;
    CBaseEntity *argument = nullptr;
    void Think() {
        ++calls;
    }
    void Touch(CBaseEntity *entity) {
        calls += 2;
        argument = entity;
    }
    void Use(CBaseEntity *activator, CBaseEntity *caller, USE_TYPE type, float value) {
        if (activator == caller && type == USE_ON && value == 1.0f) {
            calls += 4;
            argument = caller;
        }
    }
};

class CBaseEntity : public Primary, public Secondary {
public:
    BASEPTR m_pfnThink = nullptr;
    ENTITYFUNCPTR m_pfnTouch = nullptr;
    USEPTR m_pfnUse = nullptr;
    ENTITYFUNCPTR m_pfnBlocked = nullptr;
#include "../../../GoldSrc-SDK/particleman/compat/prediction_callbacks.h"
};

int main() {
    CBaseEntity entity;
    const auto think = static_cast<BASEPTR>(&Secondary::Think);
    const auto touch = static_cast<ENTITYFUNCPTR>(&Secondary::Touch);
    const auto use = static_cast<USEPTR>(&Secondary::Use);
    if (entity.ThinkSet(think, "Think") != think || entity.TouchSet(touch, "Touch") != touch
        || entity.UseSet(use, "Use") != use || entity.BlockedSet(touch, "Blocked") != touch) {
        return 1;
    }
    (entity.*entity.m_pfnThink)();
    (entity.*entity.m_pfnTouch)(&entity);
    (entity.*entity.m_pfnUse)(&entity, &entity, USE_ON, 1.0f);
    (entity.*entity.m_pfnBlocked)(&entity);
    if (entity.calls != 9 || entity.argument != &entity || entity.ThinkSet(nullptr, "Think") != nullptr
        || entity.TouchSet(nullptr, "Touch") != nullptr || entity.UseSet(nullptr, "Use") != nullptr
        || entity.BlockedSet(nullptr, "Blocked") != nullptr || entity.m_pfnThink || entity.m_pfnTouch
        || entity.m_pfnUse || entity.m_pfnBlocked) {
        return 1;
    }
    std::puts("Prediction setters preserve adjusted member callbacks and typed nulls");
    return 0;
}
