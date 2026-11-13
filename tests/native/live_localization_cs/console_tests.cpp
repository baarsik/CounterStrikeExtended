#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <strings.h>
#include <vector>

using qboolean = int;
#include "localized_titles.h"
#include "resource_text.h"

#define Q_strlen std::strlen
#define Q_strncpy std::strncpy
#define Q_strcpy std::strcpy
#define Q_memcpy std::memcpy
#define Q_strnicmp strncasecmp
#define Q_strcasecmp strcasecmp
#define Q_atof std::atof

void Con_DPrintf(const char*, ...) {}
void Sys_Error(const char*, ...) { std::abort(); }
void* Mem_Malloc(std::size_t size) { return std::malloc(size); }
void Mem_Free(void* pointer) { std::free(pointer); }
void TextMessageRetireLanguageTables() {}
std::vector<byte> loadedFile;
byte* COM_LoadTempFile(char*, int* size) {
    *size = static_cast<int>(loadedFile.size());
    return loadedFile.empty() ? nullptr : loadedFile.data();
}
struct EngineDestination {
    void pfnTextMessageGet(const char**) {}
} g_engdstAddrs;
client_textmessage_t tm_demomessage{};
client_textmessage_t gNetworkTextMessage[4]{};
client_textmessage_t gMessageParms{};
client_textmessage_t* gMessageTable = nullptr;
int gMessageTableCount = 0;
#include "engine.inc"

constexpr int HUD_PRINTNOTIFY = 1;
constexpr int HUD_PRINTCONSOLE = 2;
constexpr int HUD_PRINTTALK = 3;
constexpr int HUD_PRINTCENTER = 4;
constexpr int ScreenHeight = 480;
std::string console;
void ConsolePrint(const char* text) { console += text; }
struct Viewport {
    bool AllowedToPrintText() { return true; }
} *gViewPortInterface = nullptr;
struct SayText {
    void SayTextPrint(const char*, int, int, char*, char*, char*, char*) {}
    void CaptureTextMessageSources(const char*, const std::string (&)[4]) {}
};
struct CenterText {
    void VGUI2HudPrintArgs(const char*, char*, char*, char*, char*, int, int, int, double, double) {}
    void CaptureTextMessageSources(const char*, const std::string (&)[4]) {}
};
struct Hud {
    SayText m_SayText;
    CenterText m_VGUI2Print;
} gHUD;
class CHudTextMessage {
public:
    char* LookupString(const char*, int* = nullptr);
    int MsgFunc_TextMsg(const char*, int, void*);
};
char* safe_strcpy(char* destination, const char* source, int size) {
    std::strncpy(destination, source, size);
    destination[size - 1] = 0;
    return destination;
}
int safe_sprintf(char* destination, int size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(destination, size, format, args);
    va_end(args);
    return result;
}
int packetDestination = HUD_PRINTCONSOLE;
int nextString = 0;
const char* packet[5]{};
#define BEGIN_READ(buffer, size) nextString = 0
#define READ_BYTE() packetDestination
#define READ_STRING() packet[nextString++]
#include "client.inc"

void Send(int target, const char* format, const char* argument = "") {
    packetDestination = target;
    packet[0] = format;
    packet[1] = argument;
    packet[2] = packet[3] = packet[4] = "";
    CHudTextMessage owner;
    assert(owner.MsgFunc_TextMsg("TextMsg", 0, nullptr) == 1);
}

std::string Read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void CheckRussianConsole() {
    console.clear();
    Send(HUD_PRINTNOTIFY, "#Game_connected", "baarsik");
    Send(HUD_PRINTNOTIFY, "#Game_join_terrorist", "baarsik");
    Send(HUD_PRINTCONSOLE, "#Game_scoring");
    Send(HUD_PRINTNOTIFY, "#Game_connected", "Graham");
    assert(console == "\1baarsik подключился\n"
        "\1baarsik вступает к террористам\n"
        "Подсчёт очков начнётся, когда в обеих командах будут игроки\n"
        "\1Graham подключился\n");
}

void CheckSyntheticConsole(bool crlf) {
    console.clear();
    Send(HUD_PRINTNOTIFY, "#NOTICE", "#ARGUMENT");
    Send(HUD_PRINTCONSOLE, "#MULTILINE");
    Send(HUD_PRINTCONSOLE, "literal fragment ");
    Send(HUD_PRINTCONSOLE, "continued\n");
    const char* expected = crlf
        ? "\1Player connected\nFirst\n\nSecond\nliteral fragment continued\n"
        : "\1Player connected\nFirst\nSecond\nliteral fragment continued\n";
    assert(console == expected);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string root = argv[1];
    const std::string russian = Read(root + "/cstrike_russian/titles.txt");
    loadedFile.assign(russian.begin(), russian.end());
    TextMessageInit();
    CheckRussianConsole();
    TextMessageShutdown();

    cse::localization::TitleSnapshots snapshots;
    assert(snapshots.Prepare(Read(root + "/cstrike/titles.txt"), russian));
    snapshots.Commit();
    gMessageTable = snapshots.Active()->Data();
    gMessageTableCount = snapshots.Active()->Count();
    CheckRussianConsole();

    // Final LF/CRLF separators match; internal line bytes retain their existing behavior.
    const std::string lf = "NOTICE\n{\n%s connected\n}\nARGUMENT\n{\nPlayer\n}\n"
        "MULTILINE\n{\nFirst\nSecond\n}\n";
    std::string crlf;
    for (char character : lf) {
        if (character == '\n') {
            crlf += '\r';
        }
        crlf += character;
    }
    for (const auto& source : {lf, crlf}) {
        gMessageTable = nullptr;
        gMessageTableCount = 0;
        loadedFile.assign(source.begin(), source.end());
        TextMessageInit();
        CheckSyntheticConsole(source == crlf);
        TextMessageShutdown();
        assert(snapshots.Prepare(source, ""));
        snapshots.Commit();
        gMessageTable = snapshots.Active()->Data();
        gMessageTableCount = snapshots.Active()->Count();
        CheckSyntheticConsole(source == crlf);
    }
    gMessageTable = nullptr;
    gMessageTableCount = 0;
    std::puts("PASS: cold/live LF and CRLF titles keep console notices on separate lines.");
}
