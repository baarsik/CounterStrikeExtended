// Shared implementation included by each game DLL's saved_callbacks.cpp.
// STL and exception use stay in this translation unit; the generated fragments
// retain their original source files' floating-point and exception policies.
namespace {

using cse::save::CallbackField;
using cse::save::CallbackRegistry;
using cse::save::CallbackStatus;

struct GameCallbacks {
    CallbackRegistry<BASEPTR> think;
    CallbackRegistry<ENTITYFUNCPTR> touch;
    CallbackRegistry<USEPTR> use;
    CallbackRegistry<ENTITYFUNCPTR> blocked;
    CallbackRegistry<CseMoveDonePointer> moveDone;
};

GameCallbacks &Callbacks() {
    static GameCallbacks callbacks;
    return callbacks;
}

template<typename Callback>
const char *FindName(const void *storage, const CallbackRegistry<Callback> &registry) {
    const auto match = registry.NameFor(*static_cast<const Callback *>(storage));
    return match.status == CallbackStatus::Found ? match.name : nullptr;
}

template<typename Callback>
bool Restore(void *storage, std::string_view name, const CallbackRegistry<Callback> &registry) {
    return CallbackField<Callback>::Restore(*static_cast<Callback *>(storage), name, registry);
}

} // namespace

bool CseRegisterThink(const char *name, BASEPTR value) {
    return Callbacks().think.Register(name, value);
}

bool CseRegisterTouch(const char *name, ENTITYFUNCPTR value) {
    return Callbacks().touch.Register(name, value);
}

bool CseRegisterUse(const char *name, USEPTR value) {
    return Callbacks().use.Register(name, value);
}

bool CseRegisterBlocked(const char *name, ENTITYFUNCPTR value) {
    return Callbacks().blocked.Register(name, value);
}

bool CseRegisterMoveDone(const char *name, CseMoveDonePointer value) {
    return Callbacks().moveDone.Register(name, value);
}

bool CseInitializeSavedCallbacks() {
    static const bool initialized = []() {
        try {
            if (!CseRegisterGameCallbacks()) {
                return false;
            }
#ifndef CSE_SAVE_GAME_CS
            // Win32 /OPT:ICF historically emitted this platform MoveDone name
            // for the monster Think wrapper. The field category disambiguates it.
            if (!Callbacks().think.Alias("CallHitBottom@CFuncPlat", "CallMonsterThink@CBaseMonster")) {
                return false;
            }
#endif
            return Callbacks().think.Seal(true) && Callbacks().touch.Seal(true) && Callbacks().use.Seal(true)
                && Callbacks().blocked.Seal(true) && Callbacks().moveDone.Seal(true);
        } catch (...) {
            return false;
        }
    }();
    return initialized;
}

CseSavedCallbackKind CseCallbackKind(const TYPEDESCRIPTION *fields, const TYPEDESCRIPTION &field) {
    if (field.fieldType != FIELD_FUNCTION || field.fieldSize != 1) {
        return CseSavedCallbackKind::Invalid;
    }
    if (fields == CBaseEntity::m_SaveData) {
        if (std::strcmp(field.fieldName, "m_pfnThink") == 0) {
            return CseSavedCallbackKind::Think;
        }
        if (std::strcmp(field.fieldName, "m_pfnTouch") == 0) {
            return CseSavedCallbackKind::Touch;
        }
        if (std::strcmp(field.fieldName, "m_pfnUse") == 0) {
            return CseSavedCallbackKind::Use;
        }
        if (std::strcmp(field.fieldName, "m_pfnBlocked") == 0) {
            return CseSavedCallbackKind::Blocked;
        }
    }
    if (fields == CBaseToggle::m_SaveData && std::strcmp(field.fieldName, "m_pfnCallWhenMoveDone") == 0) {
        return CseSavedCallbackKind::MoveDone;
    }
    return CseSavedCallbackKind::Invalid;
}

bool CseCallbackEmpty(CseSavedCallbackKind kind, const void *storage) {
    switch (kind) {
    case CseSavedCallbackKind::Think:
        return CallbackField<BASEPTR>::IsEmpty(*static_cast<const BASEPTR *>(storage));
    case CseSavedCallbackKind::Touch:
    case CseSavedCallbackKind::Blocked:
        return CallbackField<ENTITYFUNCPTR>::IsEmpty(*static_cast<const ENTITYFUNCPTR *>(storage));
    case CseSavedCallbackKind::Use:
        return CallbackField<USEPTR>::IsEmpty(*static_cast<const USEPTR *>(storage));
    case CseSavedCallbackKind::MoveDone:
        return CallbackField<CseMoveDonePointer>::IsEmpty(*static_cast<const CseMoveDonePointer *>(storage));
    default:
        return false;
    }
}

bool CseClearCallback(CseSavedCallbackKind kind, void *storage) {
    switch (kind) {
    case CseSavedCallbackKind::Think:
        CallbackField<BASEPTR>::Clear(*static_cast<BASEPTR *>(storage));
        break;
    case CseSavedCallbackKind::Touch:
    case CseSavedCallbackKind::Blocked:
        CallbackField<ENTITYFUNCPTR>::Clear(*static_cast<ENTITYFUNCPTR *>(storage));
        break;
    case CseSavedCallbackKind::Use:
        CallbackField<USEPTR>::Clear(*static_cast<USEPTR *>(storage));
        break;
    case CseSavedCallbackKind::MoveDone:
        CallbackField<CseMoveDonePointer>::Clear(*static_cast<CseMoveDonePointer *>(storage));
        break;
    default:
        return false;
    }
    return true;
}

const char *CseCallbackName(CseSavedCallbackKind kind, const void *storage) {
    switch (kind) {
    case CseSavedCallbackKind::Think:
        return FindName<BASEPTR>(storage, Callbacks().think);
    case CseSavedCallbackKind::Touch:
        return FindName<ENTITYFUNCPTR>(storage, Callbacks().touch);
    case CseSavedCallbackKind::Blocked:
        return FindName<ENTITYFUNCPTR>(storage, Callbacks().blocked);
    case CseSavedCallbackKind::Use:
        return FindName<USEPTR>(storage, Callbacks().use);
    case CseSavedCallbackKind::MoveDone:
        return FindName<CseMoveDonePointer>(storage, Callbacks().moveDone);
    default:
        return nullptr;
    }
}

bool CseRestoreCallback(CseSavedCallbackKind kind, void *storage, const void *bytes, int size, [[maybe_unused]] const void *base) {
    // A callback payload is one complete NUL-terminated symbol. Never read
    // beyond the field envelope, including on malformed/truncated input.
    if (size <= 0 || !bytes || static_cast<const char *>(bytes)[size - 1] != '\0'
        || std::memchr(bytes, '\0', size) != static_cast<const char *>(bytes) + size - 1) {
        CseClearCallback(kind, storage);
        return false;
    }
    std::string_view name(static_cast<const char *>(bytes), static_cast<std::size_t>(size - 1));
#ifdef CSE_SAVE_GAME_CS
    if (kind == CseSavedCallbackKind::Think && name == "SUB_DoNothing@CBaseEntity") {
        // ENTVARS is restored before BASE. Smack and SUB_DoNothing were folded
        // in legacy Win32; only the knife assigns Smack in this source tree.
        const auto *entity = static_cast<const CBaseEntity *>(base);
        if (entity && entity->pev && std::strcmp(STRING(entity->pev->classname), "weapon_knife") == 0) {
            name = "Smack@CKnife";
        }
    }
#endif
    switch (kind) {
    case CseSavedCallbackKind::Think:
        return Restore<BASEPTR>(storage, name, Callbacks().think);
    case CseSavedCallbackKind::Touch:
        return Restore<ENTITYFUNCPTR>(storage, name, Callbacks().touch);
    case CseSavedCallbackKind::Blocked:
        return Restore<ENTITYFUNCPTR>(storage, name, Callbacks().blocked);
    case CseSavedCallbackKind::Use:
        return Restore<USEPTR>(storage, name, Callbacks().use);
    case CseSavedCallbackKind::MoveDone:
        return Restore<CseMoveDonePointer>(storage, name, Callbacks().moveDone);
    default:
        return false;
    }
}

namespace {

template<typename Callback>
void CheckCallback(Callback value, const CallbackRegistry<Callback> &registry,
    const char *name, const char *entity) {
    if (value && registry.NameFor(value).status != CallbackStatus::Found) {
        ALERT(at_error, "Unregistered or ambiguous callback: %s:%s\n", entity, name);
    }
}

} // namespace

void CseCheckThink(BASEPTR value, const char *name, const char *entity) {
    CheckCallback(value, Callbacks().think, name, entity);
}

void CseCheckTouch(ENTITYFUNCPTR value, const char *name, const char *entity) {
    CheckCallback(value, Callbacks().touch, name, entity);
}

void CseCheckUse(USEPTR value, const char *name, const char *entity) {
    CheckCallback(value, Callbacks().use, name, entity);
}

void CseCheckBlocked(ENTITYFUNCPTR value, const char *name, const char *entity) {
    CheckCallback(value, Callbacks().blocked, name, entity);
}
