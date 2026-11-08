#include "../../../GoldSrc-SDK/common/saved_callbacks.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

// Compile the production erased-field bridge with small owned entity types.
// This tests the actual implementation without launching an engine or Steam.
enum FIELD_TYPE { FIELD_INTEGER, FIELD_FUNCTION };
struct TYPEDESCRIPTION {
    FIELD_TYPE fieldType;
    const char *fieldName;
    int fieldSize;
};
enum USE_TYPE { USE_OFF, USE_ON };
struct EntityVars { const char *classname; };
class CBaseEntity {
public:
    EntityVars *pev = nullptr;
    int calls = 0;
    static TYPEDESCRIPTION m_SaveData[];
    void Think() { ++calls; }
    void Noop() {}
    void Smack() { calls += 2; }
    void Touch(CBaseEntity *) { calls += 3; }
    void Use(CBaseEntity *, CBaseEntity *, USE_TYPE, float) { calls += 4; }
};
class CBaseToggle : public CBaseEntity {
public:
    static TYPEDESCRIPTION m_SaveData[];
    void Done() { ++calls; }
};
using BASEPTR = void (CBaseEntity::*)();
using ENTITYFUNCPTR = void (CBaseEntity::*)(CBaseEntity *);
using USEPTR = void (CBaseEntity::*)(CBaseEntity *, CBaseEntity *, USE_TYPE, float);

#include "../../../GoldSrc-SDK/common/game_saved_callbacks.h"

TYPEDESCRIPTION CBaseEntity::m_SaveData[] = {
    {FIELD_FUNCTION, "m_pfnThink", 1},
    {FIELD_FUNCTION, "m_pfnTouch", 1},
    {FIELD_FUNCTION, "m_pfnUse", 1},
    {FIELD_FUNCTION, "m_pfnBlocked", 1}
};
TYPEDESCRIPTION CBaseToggle::m_SaveData[] = {{FIELD_FUNCTION, "m_pfnCallWhenMoveDone", 1}};

static bool CseRegisterGameCallbacks() {
    return CseRegisterThink("CallMonsterThink@CBaseMonster", &CBaseEntity::Think)
        && CseRegisterThink("SUB_DoNothing@CBaseEntity", &CBaseEntity::Noop)
        && CseRegisterThink("Smack@CKnife", &CBaseEntity::Smack)
        && CseRegisterTouch("Touch@CBaseEntity", &CBaseEntity::Touch)
        && CseRegisterUse("Use@CBaseEntity", &CBaseEntity::Use)
        && CseRegisterMoveDone("CallHitBottom@CFuncPlat", &CBaseToggle::Done);
}

static int diagnostics = 0;
enum { at_error = 0 };
static void Alert(int, const char *, ...) { ++diagnostics; }
#define ALERT Alert
#define STRING(value) (value)
#include "../../../GoldSrc-SDK/common/game_saved_callbacks_impl.h"

int main() {
    using Kind = CseSavedCallbackKind;
    BASEPTR think = &CBaseEntity::Think;
    // A restore before eager initialization must fail and clear the whole field.
    const char monsterName[] = "CallMonsterThink@CBaseMonster";
    CHECK(!CseRestoreCallback(Kind::Think, &think, monsterName, sizeof(monsterName), nullptr));
    CHECK(think == nullptr);
    CHECK(CseInitializeSavedCallbacks());
    CHECK(CseInitializeSavedCallbacks());
    CHECK(!CseRegisterThink("TooLate", &CBaseEntity::Think));

    const Kind kinds[] = {Kind::Think, Kind::Touch, Kind::Use, Kind::Blocked};
    for (int index = 0; index < 4; ++index) {
        CHECK(CseCallbackKind(CBaseEntity::m_SaveData, CBaseEntity::m_SaveData[index]) == kinds[index]);
    }
    CHECK(CseCallbackKind(CBaseToggle::m_SaveData, CBaseToggle::m_SaveData[0]) == Kind::MoveDone);
    TYPEDESCRIPTION unrelated[] = {{FIELD_FUNCTION, "m_pfnThink", 1}};
    CHECK(CseCallbackKind(unrelated, unrelated[0]) == Kind::Invalid);
    auto invalidArray = CBaseEntity::m_SaveData[0];
    invalidArray.fieldSize = 2;
    CHECK(CseCallbackKind(CBaseEntity::m_SaveData, invalidArray) == Kind::Invalid);

    struct Storage {
        std::uint64_t before = UINT64_C(0x1234567812345678);
        BASEPTR think = &CBaseEntity::Smack;
        ENTITYFUNCPTR touch = &CBaseEntity::Touch;
        USEPTR use = &CBaseEntity::Use;
        ENTITYFUNCPTR blocked = &CBaseEntity::Touch;
        CseMoveDonePointer done = &CBaseToggle::Done;
        std::uint64_t after = UINT64_C(0x8765432187654321);
    };
    auto fields = std::make_unique<Storage>();
    CHECK(CseClearCallback(Kind::Think, &fields->think));
    CHECK(CseClearCallback(Kind::Touch, &fields->touch));
    CHECK(CseClearCallback(Kind::Use, &fields->use));
    CHECK(CseClearCallback(Kind::Blocked, &fields->blocked));
    CHECK(CseClearCallback(Kind::MoveDone, &fields->done));
    CHECK(fields->think == nullptr && fields->touch == nullptr && fields->use == nullptr
        && fields->blocked == nullptr && fields->done == nullptr);
    CHECK(CseCallbackEmpty(Kind::Think, &fields->think));
    CHECK(CseCallbackEmpty(Kind::Touch, &fields->touch));
    CHECK(CseCallbackEmpty(Kind::Use, &fields->use));
    CHECK(CseCallbackEmpty(Kind::Blocked, &fields->blocked));
    CHECK(CseCallbackEmpty(Kind::MoveDone, &fields->done));
    CHECK(fields->before == UINT64_C(0x1234567812345678));
    CHECK(fields->after == UINT64_C(0x8765432187654321));

    EntityVars vars{"monster_scientist"};
    CBaseEntity entity;
    entity.pev = &vars;
    CHECK(CseRestoreCallback(Kind::Think, &fields->think, monsterName, sizeof(monsterName), &entity));
    (entity.*fields->think)();
    CHECK(entity.calls == 1);
    CHECK(std::strcmp(CseCallbackName(Kind::Think, &fields->think), monsterName) == 0);
    const char legacyName[] = "CallHitBottom@CFuncPlat";
#ifndef CSE_SAVE_GAME_CS
    CHECK(CseRestoreCallback(Kind::Think, &fields->think, legacyName, sizeof(legacyName), &entity));
    CHECK(fields->think == &CBaseEntity::Think);
#else
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, legacyName, sizeof(legacyName), &entity));
    const char noOpName[] = "SUB_DoNothing@CBaseEntity";
    vars.classname = "weapon_knife";
    CHECK(CseRestoreCallback(Kind::Think, &fields->think, noOpName, sizeof(noOpName), &entity));
    CHECK(fields->think == &CBaseEntity::Smack);
    vars.classname = "infodecal";
    CHECK(CseRestoreCallback(Kind::Think, &fields->think, noOpName, sizeof(noOpName), &entity));
    CHECK(fields->think == &CBaseEntity::Noop);
#endif
    CHECK(CseRestoreCallback(Kind::MoveDone, &fields->done, legacyName, sizeof(legacyName), &entity));
    CHECK(fields->done == &CBaseToggle::Done);

    // Bounds checks precede name lookup; a missing terminator cannot overread.
    const char malformed[] = {'b', 'a', 'd'};
    const char trailing[] = {'\0', 'x', '\0'};
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, malformed, sizeof(malformed), &entity));
    CHECK(fields->think == nullptr);
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, trailing, sizeof(trailing), &entity));
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, nullptr, 0, &entity));
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, nullptr, -1, &entity));
    const char empty[] = "";
    CHECK(CseRestoreCallback(Kind::Think, &fields->think, empty, sizeof(empty), &entity));
    const char missing[] = "Unknown@CBaseEntity";
    CHECK(!CseRestoreCallback(Kind::Think, &fields->think, missing, sizeof(missing), &entity));
    CHECK(fields->think == nullptr);
    CHECK(!CseClearCallback(Kind::Invalid, &fields->think));
    CHECK(diagnostics == 0);
    return 0;
}
