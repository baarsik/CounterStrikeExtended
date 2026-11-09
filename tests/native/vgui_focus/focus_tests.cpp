#ifdef CSE_FOCUS_PUBLIC
#include "KeyValues.h"
#else
#include "vgui/KeyValues.h"
#endif
#include "IKeyValues.h"
#include "cse_vgui.h"
#include "vgui/MessageMap.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

#define ASSERT_AT(expression, line) do { if (!(expression)) { std::abort(); } } while (false)

namespace {
std::deque<std::string>& Symbols() {
    static std::deque<std::string> symbols;
    return symbols;
}

int allocations = 0;
}

// Controlled service boundary: production KeyValues keeps its allocation, type,
// copy, and lifetime code. This service supplies allocation and interned symbols.
#ifdef CSE_FOCUS_PUBLIC
class KeyValuesService : public IKeyValues {
public:
    void RegisterSizeofKeyValues(int) override;
    void* AllocKeyValuesMemory(int size) override;
    void FreeKeyValuesMemory(void* memory) override;
    HKeySymbol GetSymbolForString(const char* name) override;
    const char* GetStringForSymbol(HKeySymbol symbol) override;
    void GetLocalizedFromANSI(const char*, wchar_t*, int) override;
    void GetANSIFromLocalized(const wchar_t*, char*, int) override;
    void AddKeyValuesToMemoryLeakList(void*, HKeySymbol) override;
    void RemoveKeyValuesFromMemoryLeakList(void*) override;
};
#else
using KeyValuesService = IKeyValues;
#endif

void KeyValuesService::RegisterSizeofKeyValues(int) {}
void* KeyValuesService::AllocKeyValuesMemory(int size) {
    ++allocations;
    return std::malloc(static_cast<std::size_t>(size));
}
void KeyValuesService::FreeKeyValuesMemory(void* memory) {
    if (memory) {
        --allocations;
        std::free(memory);
    }
}
HKeySymbol KeyValuesService::GetSymbolForString(const char* name) {
    auto& symbols = Symbols();
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        if (symbols[index] == name) {
            return static_cast<int>(index);
        }
    }
    symbols.emplace_back(name);
    return static_cast<int>(symbols.size() - 1);
}
const char* KeyValuesService::GetStringForSymbol(HKeySymbol symbol) {
    return Symbols().at(static_cast<std::size_t>(symbol)).c_str();
}
void KeyValuesService::GetLocalizedFromANSI(const char*, wchar_t*, int) { std::abort(); }
void KeyValuesService::GetANSIFromLocalized(const wchar_t*, char*, int) { std::abort(); }
void KeyValuesService::AddKeyValuesToMemoryLeakList(void*, HKeySymbol) {}
void KeyValuesService::RemoveKeyValuesFromMemoryLeakList(void*) {}
IKeyValues* keyvalues() {
    static KeyValuesService service;
    return &service;
}

namespace vgui2 {
class IVGui {
public:
    void DPrintf(const char*, ...) { std::abort(); }
};

IVGui* ivgui() {
    static IVGui gui;
    return &gui;
}

// Controlled Panel bindings exclude OS rendering and UI creation. The encoder,
// registration macro, and both complete message dispatch methods come unchanged
// from production. A derived receiver proves virtual member calls survive storage.
class Panel {
    DECLARE_CLASS_SIMPLE_NOBASE(Panel);
public:
    virtual ~Panel() {}
    #include "focus_map.inc"
    virtual void ReceiveSingle(VPANEL) {}
    void OnMessage(const KeyValues* message, VPANEL sender);
    void OnOldMessage(KeyValues* message, VPANEL sender);
    void PreparePanelMap(PanelMap_t* map);
    PanelMap_t* GetPanelMap() { return oldMap; }
    const char* GetName() { return "fixture"; }
    void CallParentFunction(KeyValues* message) {
        if (copyMessage) {
            KeyValues* copy = message->MakeCopy();
            message->deleteThis();
            message = copy;
        }
        parent->OnMessage(message, 0);
        message->deleteThis();
    }

    Panel* parent = nullptr;
    PanelMap_t* oldMap = nullptr;
    bool copyMessage = false;
    static PanelMap_t m_PanelMap;
};

PanelMap_t Panel::m_PanelMap{};

PanelMessageMap* FindOrAddPanelMessageMap(const char*) {
    static PanelMessageMap map{};
    return &map;
}
}

#if defined(CSE_FOCUS_CS) || defined(CSE_FOCUS_PUBLIC)
#include "cs_bodies.inc"
#else
#include "shared_bodies.inc"
#endif

namespace {
using namespace vgui2;

class Receiver : public Panel {
public:
    void OnRequestFocus(VPANEL subFocus, VPANEL defaultPanel) override {
        first = subFocus;
        second = defaultPanel;
        ++calls;
    }
    void ReceiveSingle(VPANEL value) override {
        first = value;
        ++calls;
    }
    VPANEL first = 0;
    VPANEL second = 0;
    int calls = 0;
};

bool Check(const Receiver& receiver, VPANEL first, VPANEL second, int calls) {
    if (receiver.first != first || receiver.second != second || receiver.calls != calls) {
        std::fprintf(stderr, "focus mismatch: got %llx/%llx calls=%d; expected %llx/%llx calls=%d\n",
            static_cast<unsigned long long>(receiver.first), static_cast<unsigned long long>(receiver.second),
            receiver.calls, static_cast<unsigned long long>(first), static_cast<unsigned long long>(second), calls);
        return false;
    }
    return true;
}

bool Forward(const char* scenario) {
    // Distinct high halves and sign-bit-set low halves catch both truncation and sign extension.
    constexpr VPANEL subFocus = static_cast<VPANEL>(UINT64_C(0x00000263dc6991d0));
    constexpr VPANEL defaultPanel = static_cast<VPANEL>(UINT64_C(0x00000731f1234560));
    Panel sender;
    Receiver receiver;
    sender.parent = &receiver;
    sender.copyMessage = std::strcmp(scenario, "copied") == 0;
    sender.OnRequestFocus(subFocus, defaultPanel);
    if (!Check(receiver, subFocus, defaultPanel, 1)) {
        return false;
    }
    if (std::strcmp(scenario, "null-default") == 0) {
        sender.OnRequestFocus(subFocus, 0);
        if (!Check(receiver, subFocus, 0, 2)) {
            return false;
        }
        KeyValues* missing = new KeyValues("OnRequestFocus");
        receiver.OnMessage(missing, 0);
        missing->deleteThis();
        return Check(receiver, 0, 0, 3);
    }
    return true;
}

bool Dispatch(bool old) {
    Receiver receiver;
    PanelMessageMap* modern = receiver.GetMessageMap();
    const int originalCount = modern->entries.Count();
    MessageMapItem_t focus = modern->entries.Element(0);
    MessageMapItem_t entries[2]{};
    std::memcpy(&entries[0], &focus, sizeof(focus));
    std::memcpy(&entries[1], &focus, sizeof(focus));
    entries[1].name = "SinglePanel";
    entries[1].numParams = 1;
    entries[1].func = reinterpret_cast<MessageFunc_t>(&Panel::ReceiveSingle);
    entries[1].secondParamType = DATATYPE_VOID;
    entries[1].secondParamName = nullptr;
    PanelMap_t oldMap{entries, 2, "fixture", nullptr, 0};
    if (old) {
        modern->entries.RemoveAll();
        receiver.oldMap = &oldMap;
    } else {
        modern->entries.AddToTail(entries[1]);
        modern->processed = false;
    }
    constexpr VPANEL highPanel = static_cast<VPANEL>(UINT64_C(0x00000567fedc1230));
    constexpr VPANEL otherPanel = static_cast<VPANEL>(UINT64_C(0x00000987ab654320));
    Panel sender;
    sender.parent = &receiver;
    sender.OnRequestFocus(highPanel, otherPanel);
    bool passed = Check(receiver, highPanel, otherPanel, 1);
    KeyValues* message = new KeyValues("SinglePanel");
    #if UINTPTR_MAX > UINT32_MAX
    message->SetUint64("subFocus", static_cast<std::uint64_t>(highPanel));
    #else
    message->SetInt("subFocus", static_cast<int>(highPanel));
    #endif
    receiver.OnMessage(message, 0);
    message->deleteThis();
    passed = Check(receiver, highPanel, otherPanel, 2) && passed;
    KeyValues* missing = new KeyValues("OnRequestFocus");
    receiver.OnMessage(missing, 0);
    missing->deleteThis();
    passed = Check(receiver, 0, 0, 3) && passed;
    if (old) {
        modern->entries.AddToTail(focus);
    } else {
        modern->entries.Remove(originalCount);
    }
    modern->processed = false;
    return passed;
}

bool CopyPointers() {
    constexpr std::uintptr_t rootAddress = static_cast<std::uintptr_t>(UINT64_C(0x00000263dc6991d0));
    constexpr std::uintptr_t childAddress = static_cast<std::uintptr_t>(UINT64_C(0x00000731f1234560));
    void* rootPointer = reinterpret_cast<void*>(rootAddress);
    void* childPointer = reinterpret_cast<void*>(childAddress);
    for (void* pointer : {rootPointer, static_cast<void*>(nullptr)}) {
        KeyValues* original = new KeyValues("PointerCopy");
        original->SetPtr("child", childPointer);
        original->SetPtr("null", nullptr);
        original->SetInt("integer", -275);
        original->SetFloat("float", 13.25f);
        original->SetPtr(nullptr, pointer);
        KeyValues* copy = original->MakeCopy();
        original->deleteThis();
        const bool passed = copy->GetPtr(nullptr, childPointer) == pointer &&
            copy->GetPtr("child", nullptr) == childPointer && copy->GetPtr("null", childPointer) == nullptr &&
            copy->GetInt("integer", 0) == -275 && copy->GetFloat("float", 0.0f) == 13.25f;
        if (!passed) {
            std::fprintf(stderr, "pointer copy mismatch: root=%p expected=%p child=%p expected=%p null=%p\n",
                copy->GetPtr(nullptr, childPointer), pointer, copy->GetPtr("child", nullptr), childPointer,
                copy->GetPtr("null", childPointer));
        }
        copy->deleteThis();
        if (!passed) {
            return false;
        }
    }
    return true;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    bool passed;
    if (std::strcmp(argv[1], "pointer-copy") == 0) {
        passed = CopyPointers();
    } else if (std::strcmp(argv[1], "modern-map") == 0 || std::strcmp(argv[1], "old-map") == 0) {
        passed = Dispatch(std::strcmp(argv[1], "old-map") == 0);
    } else {
        passed = Forward(argv[1]);
    }
    if (!passed || allocations != 0) {
        std::fprintf(stderr, "outstanding_keyvalues_allocations=%d\n", allocations);
        return 1;
    }
    std::puts("vgui_focus_passed=1");
    return 0;
}
