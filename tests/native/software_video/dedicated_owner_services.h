#pragma once
#include "interface.h"
#include "native_dedicated_lifetime.h"
#include "native_engine_arguments.h"
#include "native_dedicated_abi.h"
#include "native_filesystem_module.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

struct Engine {
    void SetQuitting(int) {}
    bool Load(bool, char *, char *) {
        return true;
    }
    void Unload();
    int GetQuitting() {
        return 0;
    }
    void Frame() {}
};
struct Game {
    bool Init(void *) {
        return true;
    }
    bool Shutdown();
};
struct Registry {
    void Init() {}
    void Shutdown();
};
static Engine engineService;
static Engine *eng = &engineService;
static Game gameService;
static Game *game = &gameService;
static Registry registryService;
static Registry *registry = &registryService;
static bool host_initialized;
static bool g_bIsDedicatedServer;
static void *videomode;
static int giActive;
static struct { const char *string = "server.cfg"; } servercfgfile;
static char *va(const char *, ...) {
    return nullptr;
}
static int build_number() {
    return 0;
}
static void SteamAPI_UseBreakpadCrashHandler(const char *, const char *, const char *, int, void *, void *) {}
static void TraceInit(const char *, const char *, int) {}
static void TraceShutdown(const char *, int) {}
static bool failFilesystemInit = false;
static bool FileSystem_Init(char *, CreateInterfaceFn) {
    return !failFilesystemInit;
}
static void Cbuf_InsertText(char *) {}
void FileSystem_Shutdown();
void Sys_ShutdownArgv();
bool CseSteamRetireServerTransport();
