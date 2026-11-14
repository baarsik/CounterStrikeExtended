#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include "extended_stats.h"
namespace cse::extstats {
ClientState& ClientStats() {
    static ClientState state;
    return state;
}
} // namespace cse::extstats
static bool polish = false;
struct VGuiLibraryPlayer_t {
    const char* name;
    int health;
};
struct ClientDll {
    VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int) {
        return {"A very long player name that must not hide numeric health or armor", 100};
    }
};
struct ViewPort {
    ClientDll client;
    ClientDll* GetClientDllInterface() {
        return &client;
    }
    void MakeSafeName(const char* in, char* out, int size) {
        std::snprintf(out, size, "%s", in);
    }
};
static ViewPort viewport;
static ViewPort* gViewPortInterface = &viewport;
namespace vgui2 {
struct Localize {
    std::wstring current;
    wchar_t* Find(const char* token) {
        const bool health = std::strstr(token, "HealthArmor") != nullptr;
        current = polish ? (health ? L"%s1 | Zycie %s2 | Pancerz %s3" : L"%s1 | Pancerz %s2")
                         : (health ? L"%s1 | HP %s2 | AP %s3" : L"%s1 | AP %s2");
        return current.data();
    }
    void ConvertANSIToUnicode(const char* in, wchar_t* out, int bytes) {
        const int limit = bytes / static_cast<int>(sizeof(wchar_t)) - 1;
        int i = 0;
        for (; in[i] && i < limit; ++i) {
            out[i] = static_cast<unsigned char>(in[i]);
        }
        out[i] = 0;
    }
    void ConstructString(wchar_t* out, int bytes, const wchar_t* format, int count, ...) {
        std::wstring result(format);
        va_list args;
        va_start(args, count);
        for (int i = 1; i <= count; ++i) {
            const wchar_t* value = va_arg(args, const wchar_t*);
            const std::wstring key = L"%s" + std::to_wstring(i);
            const auto position = result.find(key);
            if (position != std::wstring::npos) {
                result.replace(position, key.size(), value);
            }
        }
        va_end(args);
        const int limit = bytes / static_cast<int>(sizeof(wchar_t)) - 1;
        std::wcsncpy(out, result.c_str(), limit);
        out[limit] = 0;
    }
};
static Localize language;
Localize* localize() {
    return &language;
}
} // namespace vgui2
class CSpectatorGUI {
  public:
    void GetPlayerLabelText(int, wchar_t* text, int bytes) {
        const int capacity = bytes / static_cast<int>(sizeof(wchar_t));
        std::wcsncpy(text, L"legacy", capacity - 1);
        text[capacity - 1] = 0;
    }
};
class CCSSpectatorGUI : public CSpectatorGUI {
  public:
    void GetPlayerLabelText(int player, wchar_t* text, int textLength);
};
#include "spectator_bodies.inc"
int main(int, char** argv) {
#ifdef CSE_SPECTATOR_STATS_MISSING
    std::fputs("FAIL: spectator stat formatting missing\n", stderr);
    return 1;
#else
    using namespace cse::extstats;
    CCSSpectatorGUI gui;
    wchar_t text[80];
    gui.GetPlayerLabelText(1, text, sizeof(text));
    if (std::wcscmp(text, L"legacy") != 0) {
        return 1;
    }
    ClientStats().Legacy(Stat::Health, 1, 100000);
    ClientStats().Legacy(Stat::Armor, 1, 2147483647);
    polish = std::strcmp(argv[1], "localized") == 0;
    gui.GetPlayerLabelText(1, text, sizeof(text));
    const bool result = std::wcslen(text) < 80 && std::wcsstr(text, L"99999") &&
                        std::wcsstr(text, polish ? L"Zycie 99999 | Pancerz 99999" : L"HP 99999 | AP 99999");
    if (!result) {
        std::fputs("FAIL: spectator fields, localization or bounds\n", stderr);
        return 1;
    }
    return 0;
#endif
}
