// External allocation, registry and logging boundaries; parsing/dispatch bodies are actual source.
#include "common/cvardef.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

using byte = unsigned char;
using uchar32 = std::uint32_t;
constexpr int COM_TOKEN_SIZE = 2048;
constexpr int MAX_CMD_ARGS = 80;
constexpr int TRUE = 1;
constexpr int FALSE = 0;
constexpr int CMD_FLAG_PRIVILEGED = 1;
constexpr int CMD_FLAG_FILTERED = 2;
constexpr int CMD_FLAG_HUD = 4;
constexpr int src_command = 0;
constexpr int ca_dedicated = 1;
constexpr int ca_disconnected = 2;
constexpr int ca_active = 3;
constexpr int ca_connected = 4;
constexpr int ca_uninitialized = 5;
using cmd_source_t = int;
struct sizebuf_t {
    byte* data;
    int cursize;
    int maxsize;
};
struct cmd_function_t {
    const char* name;
    int flags;
    void (*function)();
    cmd_function_t* next;
};
struct cmdalias_t {
    const char* name;
    const char* value;
    cmdalias_t* next;
};
static byte commandStorage[16384];
static sizebuf_t cmd_text{commandStorage, 0, sizeof(commandStorage)};
static sizebuf_t cmd_text_untrusted{};
static cmd_function_t* cmd_functions;
static cmdalias_t* cmd_alias;
static int cmd_source;
static int cmd_argc;
static char* cmd_argv[MAX_CMD_ARGS];
static char* cmd_args;
static int cmd_wait;
static bool s_bCurrentCommandIsPrivileged;
static bool s_com_token_unget;
static bool com_ignorecolons;
static char com_token[COM_TOKEN_SIZE];
static struct {
    int state = ca_dedicated;
    bool demorecording = false;
    bool spectator = false;
} cls;
static struct {
    int maxclients = 1;
} cl;
static std::map<std::string, std::string> values;
static cvar_t hostname{const_cast<char*>("hostname"), const_cast<char*>(""), 0, 0, nullptr};
static int Cmd_Argc() {
    return cmd_argc;
}
static const char* Cmd_Argv(int index) {
    return index < cmd_argc ? cmd_argv[index] : "";
}
static void* Z_Malloc(int bytes) {
    return std::malloc(static_cast<std::size_t>(bytes));
}
static void Z_Free(void* pointer) {
    std::free(pointer);
}
static int Q_strlen(const char* text) {
    return static_cast<int>(std::strlen(text));
}
static void Q_strcpy(char* to, const char* from) {
    std::strcpy(to, from);
}
[[maybe_unused]] static void Q_strcat(char* to, const char* from) {
    std::strcat(to, from);
}
static void Q_strncpy(char* to, const char* from, int count) {
    std::strncpy(to, from, static_cast<std::size_t>(count));
}
static int Q_strcasecmp(const char* a, const char* b) {
    return strcasecmp(a, b);
}
static const char* Q_stristr(const char* text, const char* value) {
    return std::strstr(text, value);
}
static float Q_atof(const char* text) {
    return static_cast<float>(std::atof(text));
}
static void Q_memcpy(void* to, const void* from, int bytes) {
    std::memcpy(to, from, static_cast<std::size_t>(bytes));
}
static void SZ_Clear(sizebuf_t* buffer) {
    buffer->cursize = 0;
}
static void SZ_Write(sizebuf_t* buffer, const void* data, int bytes) {
    assert(buffer->cursize + bytes < buffer->maxsize);
    Q_memcpy(buffer->data + buffer->cursize, data, bytes);
    buffer->cursize += bytes;
}
static cvar_t* Cvar_FindVar(const char* name) {
    return std::strcmp(name, "hostname") == 0 ? &hostname : nullptr;
}
static float Cvar_VariableValue(const char*) {
    return 0;
}
static void Cvar_Set(const char* name, const char* value) {
    values[name] = value;
}
static bool Host_IsServerActive() {
    return false;
}
static void Con_Printf(const char*, ...) {}
static void CL_RecordHUDCommand(const char*) {}
static void Cmd_ForwardToServer() {
    std::abort();
}
#define ARRAYSIZE(value) (sizeof(value) / sizeof((value)[0]))
void Cbuf_InsertTextToBuffer(const char*, sizebuf_t*, qboolean);
void Cmd_ExecuteStringWithPrivilegeCheck(const char*, cmd_source_t, qboolean);
qboolean Cvar_CommandWithPrivilegeCheck(qboolean);
