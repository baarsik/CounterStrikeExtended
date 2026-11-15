#include "localization_message_snapshot.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>

#define ARRAYSIZE(value) (sizeof(value) / sizeof(value[0]))

constexpr int ScreenWidth = 1280;
constexpr int ScreenHeight = 720;
constexpr int HUD_ACTIVE = 1;

struct client_textmessage_t {
    int effect = 0;
    unsigned char r1 = 255;
    unsigned char g1 = 255;
    unsigned char b1 = 255;
    unsigned char a1 = 255;
    unsigned char r2 = 0;
    unsigned char g2 = 0;
    unsigned char b2 = 0;
    unsigned char a2 = 0;
    float x = -1;
    float y = 0.7f;
    float fadein = 0;
    float fadeout = 0;
    float holdtime = 0;
    float fxtime = 0;
    const char* pName = nullptr;
    const char* pMessage = nullptr;
};

std::map<std::string, client_textmessage_t> titles;
bool polish = false;
std::wstring drawn;

client_textmessage_t* TextMessageGet(const char* name) {
    const auto found = titles.find(name ? name : "");
    return found == titles.end() ? nullptr : &found->second;
}

bool CS_IsCatalogueMessage(const client_textmessage_t* message) {
    for (const auto& entry : titles) {
        if (&entry.second == message) {
            return true;
        }
    }
    return false;
}

bool CS_HasLanguageService() {
    return true;
}

namespace vgui2 {
using HFont = unsigned int;

struct IScheme {
    HFont GetFont(const char* name, bool) {
        return std::strcmp(name, "CreditsFont") == 0 ? 4 : 3;
    }
};

struct SchemeManager {
    IScheme value;
    int GetDefaultScheme() {
        return 0;
    }
    IScheme* GetIScheme(int) {
        return &value;
    }
};

SchemeManager* scheme() {
    static SchemeManager manager;
    return &manager;
}

struct Localize {
    wchar_t* Find(const char* key) {
        if (std::strcmp(key, "#Cstrike_TitlesTXT_Spec_Duck") == 0) {
            return const_cast<wchar_t*>(polish ? L"Nacisnij KUCANIE dla menu obserwatora"
                                              : L"Press DUCK for Spectator Menu");
        }
        if (std::strcmp(key, "#TipResource") == 0) {
            return const_cast<wchar_t*>(polish ? L"Inna wskazowka" : L"Another hint");
        }
        if (std::strcmp(key, "#Spec_Duck") == 0) {
            return const_cast<wchar_t*>(L"Short token translation");
        }
        return nullptr;
    }
    void ConvertANSIToUnicode(const char* text, wchar_t* output, int bytes) {
        const int capacity = bytes / static_cast<int>(sizeof(wchar_t));
        int index = 0;
        while (text && text[index] && index + 1 < capacity) {
            output[index] = static_cast<unsigned char>(text[index]);
            ++index;
        }
        output[index] = 0;
    }
    void ConstructString(wchar_t* output, int bytes, const wchar_t* format, int,
        const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*) {
        const int capacity = bytes / static_cast<int>(sizeof(wchar_t));
        std::wcsncpy(output, format ? format : L"", capacity - 1);
        output[capacity - 1] = 0;
    }
};

Localize* localize() {
    static Localize instance;
    return &instance;
}

struct Surface {
    void GetCharABCwide(HFont, wchar_t, int& before, int& width, int& after) {
        before = 0;
        width = 1;
        after = 0;
    }
    int GetFontTall(HFont) {
        constexpr int FontHeight = 12;
        return FontHeight;
    }
};

Surface* surface() {
    static Surface instance;
    return &instance;
}
} // namespace vgui2

vgui2::HFont _font = 1;
vgui2::HFont _Newfont = 2;
int g_iTeamNumber = 0;

struct Engine {
    bool spectateOnly = false;
    bool IsSpectateOnly() {
        return spectateOnly;
    }
    void Con_DPrintf(const char*, const char*) {}
    void pfnVGUI2DrawCharacterAdd(int, int, int character, int, int, int, unsigned int) {
        drawn += static_cast<wchar_t>(character);
    }
} gEngfuncs;

struct MessageParameters {
    client_textmessage_t* pMessage = nullptr;
    float time = 0;
    int x = 0;
    int y = 0;
    int totalWidth = 0;
    int totalHeight = 0;
    int width = 0;
    int lines = 0;
    int lineLength = 0;
    int length = 0;
    int r = 0;
    int g = 0;
    int b = 0;
    int text = 0;
    int fadeBlend = 0;
    float charTime = 0;
    float fadeTime = 0;
};

struct CHudMessage {
    static constexpr int MESSAGE_SLOT_COUNT = 16;
    static constexpr int MessageArguments = 4;
    static constexpr int ArgumentCharacters = 64;
    struct client_message_t {
        client_textmessage_t* pMessage = nullptr;
        unsigned int font = 0;
        wchar_t args[MessageArguments][ArgumentCharacters] = {};
        int numArgs = 0;
    };
    client_message_t m_pMessages[MESSAGE_SLOT_COUNT] = {};
    cse::localization::MessageSnapshot<client_textmessage_t> m_messageSnapshots[MESSAGE_SLOT_COUNT];
    cse::localization::MessageSnapshot<client_textmessage_t> m_gameTitleSnapshot;
    float m_startTime[MESSAGE_SLOT_COUNT] = {};
    float m_messageEndTimes[MESSAGE_SLOT_COUNT] = {};
    MessageParameters m_parms;
    client_textmessage_t* m_pGameTitle = nullptr;
    int m_iFlags = 0;
    int MessageAdd(const char*, float, int, unsigned int);
    void MessageAdd(client_textmessage_t*);
    void MessageAddLocal(client_textmessage_t*);
    void QueueMessage(const client_textmessage_t*, const client_textmessage_t*);
    float MessageEndTime(int index, const client_textmessage_t* presentation = nullptr) const;
    void RefreshLanguage();
    void MessageDrawScan(client_message_t*, float, unsigned int);
    void MessageScanStart();
    void MessageScanNextChar(unsigned int);
    int XPosition(float, int, int);
    int YPosition(float, int);
};

struct Hud {
    float m_flTime = 10;
    CHudMessage m_Message;
} gHUD;

void MessageAdd(void* message);
struct ClientInterface {
    void (*MessageAdd)(void*) = ::MessageAdd;
};
struct Viewport {
    ClientInterface client;
    ClientInterface* GetClientDllInterface() {
        return &client;
    }
} viewport;
Viewport* gViewPortInterface = &viewport;

#include "hint_bodies.inc"

std::wstring Render(CHudMessage& owner, int index) {
    drawn.clear();
    owner.MessageDrawScan(&owner.m_pMessages[index], 1.0f, owner.m_pMessages[index].font);
    return drawn;
}

int main() {
    titles["Spec_Duck"].pName = "Spec_Duck";
    titles["Spec_Duck"].pMessage = "#Cstrike_TitlesTXT_Spec_Duck\r";
    titles["Tip"].pName = "Tip";
    titles["Tip"].pMessage = "#TipResource\r";
    CHudMessage serverHint;
    const int serverSlot = serverHint.MessageAdd("#Spec_Duck", gHUD.m_flTime, 1, _font);
    assert(serverSlot == 0);
    assert(Render(serverHint, serverSlot) == L"Press DUCK for Spectator Menu");
    assert(serverHint.m_messageSnapshots[serverSlot].HasCatalogueSource());

    CHudMessage& owner = gHUD.m_Message;
    DuckMessage("#Spec_Duck");
    assert(Render(owner, 0) == L"Press DUCK for Spectator Menu");
    assert(owner.m_messageSnapshots[0].HasCatalogueSource());
    assert(owner.m_pMessages[0].pMessage->holdtime == 6.0f);
    assert(owner.m_pMessages[0].pMessage->r1 == 40 && owner.m_pMessages[0].pMessage->g1 == 255);
    assert(titles["Spec_Duck"].effect == 0 && titles["Spec_Duck"].holdtime == 0.0f);
    const float deadline = owner.m_messageEndTimes[0];
    assert(std::fabs(deadline - 16.8f) < 0.00001f);
    const float start = owner.m_startTime[0];

    client_textmessage_t tip = *owner.m_pMessages[0].pMessage;
    tip.pName = "Local caption";
    tip.pMessage = "#Tip";
    tip.x = 0.25f;
    tip.holdtime = 2.5f;
    MessageAdd(&tip);
    assert(Render(owner, 1) == L"Another hint");
    assert(owner.m_pMessages[1].pMessage->x == 0.25f && owner.m_pMessages[1].pMessage->holdtime == 2.5f);

    client_textmessage_t director = tip;
    director.pName = "Spec_Duck";
    director.pMessage = "#Spec_Duck";
    owner.MessageAdd(&director);
    assert(Render(owner, 2) == L"#Spec_Duck");
    assert(!owner.m_messageSnapshots[2].HasCatalogueSource());

    client_textmessage_t unknown = tip;
    unknown.pMessage = "#UnknownTitle";
    MessageAdd(&unknown);
    assert(Render(owner, 3) == L"#UnknownTitle");
    assert(!owner.m_messageSnapshots[3].HasCatalogueSource());

    polish = true;
    owner.RefreshLanguage();
    assert(Render(owner, 0) == L"Nacisnij KUCANIE dla menu obserwatora");
    assert(Render(owner, 1) == L"Inna wskazowka");
    assert(Render(owner, 2) == L"#Spec_Duck");
    assert(owner.m_messageEndTimes[0] == deadline && owner.m_startTime[0] == start);
    assert(owner.m_pMessages[0].pMessage->holdtime == 6.0f);

    for (int team : {1, 2}) {
        g_iTeamNumber = team;
        DuckMessage("#Spec_Duck");
        assert(owner.m_pMessages[4].pMessage == nullptr);
    }
    gEngfuncs.spectateOnly = true;
    DuckMessage("#Spec_Duck");
    assert(Render(owner, 4) == L"Nacisnij KUCANIE dla menu obserwatora");
    std::puts("PASS: local hints translate and switch languages; style/timing and literal director text are retained");
}
