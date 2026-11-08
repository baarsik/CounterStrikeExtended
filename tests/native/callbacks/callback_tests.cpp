#include "../../../GoldSrc-SDK/common/saved_callbacks.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    return false; \
} } while (false)

using cse::save::CallbackField;
using cse::save::CallbackRegistry;
using cse::save::CallbackStatus;

enum class UseType { On, Off };

struct Entity {
    virtual ~Entity() = default;
    int calls = 0;
    void Think() { ++calls; }
    void OtherThink() { calls += 2; }
    void Noop() {}
    void Touch(Entity *other) { calls += other ? other->calls : 0; }
    void Use(Entity *activator, Entity *caller, UseType type, float value) {
        calls += activator && caller && type == UseType::On && value == 0.5f ? 1 : 0;
    }
};

struct Toggle : Entity {
    void Done() { ++calls; }
};

using Think = void (Entity::*)();
using Touch = void (Entity::*)(Entity *);
using Use = void (Entity::*)(Entity *, Entity *, UseType, float);
using MoveDone = void (Toggle::*)();

static bool CheckSignaturesAndLegacyAlias() {
    // Registration completes before any entity or assignment is constructed.
    CallbackRegistry<Think> think;
    CHECK(think.Register("CallMonsterThink@CBaseMonster", &Entity::Think));
    CHECK(think.Alias("CallHitBottom@CFuncPlat", "CallMonsterThink@CBaseMonster"));
    CHECK(think.Resolve("CallHitBottom@CFuncPlat").status == CallbackStatus::NotReady);
    CHECK(think.Seal());
    CHECK(!think.Register("Late@Entity", &Entity::OtherThink));
    CHECK(!think.Alias("LateAlias", "CallMonsterThink@CBaseMonster"));
    CHECK(think.Resolve("callmonsterthink@CBaseMonster").status == CallbackStatus::Missing);

    CallbackRegistry<Touch> touch;
    CHECK(touch.Register("Touch@Entity", &Entity::Touch));
    CHECK(touch.Seal());
    CallbackRegistry<Use> use;
    CHECK(use.Register("Use@Entity", &Entity::Use));
    CHECK(use.Seal());
    CallbackRegistry<MoveDone> done;
    CHECK(done.Register("CallHitBottom@CFuncPlat", &Toggle::Done));
    CHECK(done.Seal());

    Entity object;
    Think storedThink = nullptr;
    CHECK(CallbackField<Think>::Restore(storedThink, "CallHitBottom@CFuncPlat", think));
    (object.*storedThink)();
    CHECK(object.calls == 1);
    const auto canonical = think.NameFor(storedThink);
    CHECK(canonical.status == CallbackStatus::Found);
    CHECK(std::strcmp(canonical.name, "CallMonsterThink@CBaseMonster") == 0);

    Entity other;
    other.calls = 3;
    Touch storedTouch = nullptr;
    CHECK(CallbackField<Touch>::Restore(storedTouch, "Touch@Entity", touch));
    (object.*storedTouch)(&other);
    Use storedUse = nullptr;
    CHECK(CallbackField<Use>::Restore(storedUse, "Use@Entity", use));
    (object.*storedUse)(&object, &other, UseType::On, 0.5f);
    CHECK(object.calls == 5);
    Toggle toggle;
    MoveDone storedDone = nullptr;
    CHECK(CallbackField<MoveDone>::Restore(storedDone, "CallHitBottom@CFuncPlat", done));
    (toggle.*storedDone)();
    CHECK(toggle.calls == 1);

    CHECK(CallbackField<Think>::NativeStride == sizeof(Think));
    CHECK(!CallbackField<Think>::IsEmpty(storedThink));
    CallbackField<Think>::Clear(storedThink);
    CHECK(CallbackField<Think>::IsEmpty(storedThink));
    CHECK(CallbackField<Think>::Restore(storedThink, "", think));
    CHECK(storedThink == nullptr);
    storedThink = &Entity::OtherThink;
    CHECK(!CallbackField<Think>::Restore(storedThink, "Missing@Entity", think));
    CHECK(storedThink == nullptr);
    CHECK(think.NameFor(nullptr).status == CallbackStatus::Empty);
    CHECK(think.NameFor(&Entity::OtherThink).status == CallbackStatus::Missing);
    CallbackField<Touch>::Clear(storedTouch);
    CallbackField<Use>::Clear(storedUse);
    CallbackField<MoveDone>::Clear(storedDone);
    CHECK(CallbackField<Touch>::IsEmpty(storedTouch));
    CHECK(CallbackField<Use>::IsEmpty(storedUse));
    CHECK(CallbackField<MoveDone>::IsEmpty(storedDone));
    return true;
}

struct Padding {
    virtual ~Padding() = default;
    std::array<int, 8> guard{};
};

struct Adjusted : Padding, Entity {};
using AdjustedThink = void (Adjusted::*)();

static bool CheckAdjustedMemberPointer() {
    const auto callback = static_cast<AdjustedThink>(&Entity::Think);
    CallbackRegistry<AdjustedThink> registry;
    CHECK(registry.Register("Think@Entity", callback));
    CHECK(registry.Seal());
    auto object = std::make_unique<Adjusted>();
    if constexpr (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(object.get()) > (std::numeric_limits<std::uint32_t>::max)());
    }
    CHECK(static_cast<void *>(static_cast<Entity *>(object.get())) != static_cast<void *>(object.get()));
    AdjustedThink stored = nullptr;
    CHECK(CallbackField<AdjustedThink>::Restore(stored, "Think@Entity", registry));
    (object.get()->*stored)();
    CHECK(object->calls == 1);
    const std::array<int, 8> expectedGuard{};
    CHECK(object->guard == expectedGuard);
    CHECK(registry.NameFor(stored).status == CallbackStatus::Found);
    CallbackField<AdjustedThink>::Clear(stored);
    CHECK(stored == nullptr);
    return true;
}

static bool CheckFoldedIdentityAndCatalogErrors() {
    CallbackRegistry<Think> registry;
    CHECK(!registry.Register("", &Entity::Think));
    CHECK(!registry.Register("Null@Entity", nullptr));
    CHECK(!registry.Register(std::string_view("bad\0name", 8), &Entity::Think));
    CHECK(registry.Register("SUB_DoNothing@CBaseEntity", &Entity::Noop));
    CHECK(registry.Register("Smack@CKnife", &Entity::Noop)); // Model Win32 /OPT:ICF, without relying on this linker.
    CHECK(!registry.Register("Smack@CKnife", &Entity::Think));
    CHECK(!registry.Alias("UnknownAlias", "Missing@Entity"));
    CHECK(!registry.Alias("Smack@CKnife", "SUB_DoNothing@CBaseEntity"));
    CHECK(registry.Seal());
    CHECK(registry.NameFor(&Entity::Noop).status == CallbackStatus::Ambiguous);
    const auto generic = registry.NameFor(&Entity::Noop, "SUB_DoNothing@CBaseEntity");
    CHECK(generic.status == CallbackStatus::Found);
    CHECK(std::strcmp(generic.name, "SUB_DoNothing@CBaseEntity") == 0);
    const auto knife = registry.NameFor(&Entity::Noop, "Smack@CKnife");
    CHECK(knife.status == CallbackStatus::Found);
    CHECK(std::strcmp(knife.name, "Smack@CKnife") == 0);
    CHECK(registry.NameFor(&Entity::Think, "Smack@CKnife").status == CallbackStatus::Missing);
    return true;
}

static int plainCalls = 0;
static void PlainCallback(int value) { plainCalls += value; }

static bool CheckPlainFunction() {
    CallbackRegistry<void (*)(int)> registry;
    CHECK(registry.Register("PlainCallback", PlainCallback));
    CHECK(registry.Seal());
    void (*stored)(int) = nullptr;
    CHECK(CallbackField<decltype(stored)>::Restore(stored, "PlainCallback", registry));
    stored(7);
    CHECK(plainCalls == 7);
    CallbackField<decltype(stored)>::Clear(stored);
    CHECK(stored == nullptr);
    return true;
}

static bool CheckOwnedNames() {
    CallbackRegistry<Think> registry;
    char name[] = "Owned@Entity";
    char alias[] = "OldOwned@Entity";
    CHECK(registry.Register(name, &Entity::Think));
    CHECK(registry.Alias(alias, name));
    std::memset(name, 'x', sizeof(name) - 1);
    std::memset(alias, 'x', sizeof(alias) - 1);
    CHECK(registry.Register("A", &Entity::OtherThink)); // Short owned strings must stay stable too.
    CHECK(registry.Register("B", &Entity::Noop));
    CHECK(registry.Seal());
    CHECK(registry.Resolve("Owned@Entity").status == CallbackStatus::Found);
    const auto legacy = registry.Resolve("OldOwned@Entity");
    CHECK(legacy.status == CallbackStatus::Found);
    CHECK(std::strcmp(legacy.name, "Owned@Entity") == 0);
    return true;
}

int main() {
    if (!CheckSignaturesAndLegacyAlias() || !CheckAdjustedMemberPointer()
        || !CheckFoldedIdentityAndCatalogErrors() || !CheckPlainFunction() || !CheckOwnedNames()) {
        return 1;
    }
    std::printf("Typed callbacks, eager catalogs, adjusted members, null clearing and legacy aliases passed (%zu-bit).\n",
                sizeof(void *) * 8);
    return 0;
}
