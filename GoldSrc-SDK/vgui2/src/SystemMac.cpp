// Native client System services. The client supplies VGUI_Surface026 through
// EngineSurface; the GDI standalone-tools surface is Windows-only.
#include "interface.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"
#include "KeyValues.h"
#include "vgui/ISystem.h"
#include "vgui/IInputInternal.h"
#include "NativeSystemClock.h"
#include "NativeClipboard.h"
#include "../../common/sdl_ui_clipboard.h"
#include "../../common/mac_settings_store.h"
#include <SDL3/SDL.h>
#include <crt_externs.h>
#include <sys/statvfs.h>
#include <spawn.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

using namespace vgui2;

class CSystem : public ISystem {
public:
    CSystem();
    ~CSystem() override;

    virtual void Shutdown() override;
    virtual void RunFrame() override;
    virtual void ShellExecute( const char* command, const char* file ) override;

    // returns the time at the start of the frame
    virtual double GetFrameTime() override;
    virtual double GetCurrentTime() override;
    virtual std::int32_t GetTimeMillis() override;

    virtual int GetClipboardTextCount() override;
    virtual void SetClipboardText( const char* text, int textLen ) override;
    virtual void SetClipboardText( const wchar_t* text, int textLen ) override;
    virtual int GetClipboardText( int offset, char* buf, int bufLen ) override;
    virtual int GetClipboardText( int offset, wchar_t* buf, int bufLen ) override;

    virtual bool SetRegistryString( const char* key, const char* value ) override;
    virtual bool GetRegistryString( const char* key, char* value, int valueLen ) override;
    virtual bool SetRegistryInteger( const char* key, int value ) override;
    virtual bool GetRegistryInteger( const char* key, int& value ) override;

    virtual KeyValues* GetUserConfigFileData( const char* dialogName, int dialogID ) override;
    virtual void SetUserConfigFile( const char* fileName, const char* pathName ) override;
    virtual void SaveUserConfigFile() override;

    virtual bool SetWatchForComputerUse( bool state ) override;
    virtual double GetTimeSinceLastUse() override;
    virtual int GetAvailableDrives( char* buf, int bufLen ) override;

    virtual bool CommandLineParamExists( const char* paramName ) override;
    virtual const char* GetFullCommandLine() override;
    virtual bool GetCurrentTimeAndDate( int* year, int* month, int* dayOfWeek,
        int* day, int* hour, int* minute, int* second ) override;

    virtual double GetFreeDiskSpace( const char* path ) override;

    virtual bool CreateShortcut( const char* linkFileName, const char* targetPath,
        const char* arguments, const char* workingDirectory, const char* iconFile ) override;
    virtual bool GetShortcutTarget( const char* linkFileName, char* targetPath,
        char* arguments, int destBufferSizes ) override;
    virtual bool ModifyShortcutTarget( const char* linkFileName, const char* targetPath,
        const char* arguments, const char* workingDirectory ) override;

    virtual bool GetCommandLineParamValue( const char* paramName, char* value, int valueBufferSize ) override;
    virtual bool DeleteRegistryKey( const char* keyName ) override;

    virtual const char* GetDesktopFolderPath() override;
    virtual KeyCode KeyCode_VirtualKeyToVGUI( int keyCode ) override;
    virtual int KeyCode_VGUIToVirtualKey( KeyCode keyCode ) override;

private:
    bool m_bStaticWatchForComputerUse = false;
    double m_StaticLastComputerUseTime = 0;
    int m_iStaticMouseOldX = 0;
    int m_iStaticMouseOldY = 0;
    double m_flFrameTime = 0;
    KeyValues* m_pUserConfigData = nullptr;
    static constexpr std::size_t ConfigPathCapacity = 4096;
    char m_szFileName[ConfigPathCapacity]{};
    char m_szPathID[ConfigPathCapacity]{};
    std::mutex m_TimeLock;
    cse_vgui2_clock::State m_clock{};
    std::string m_commandLine;
    std::string m_desktop;
};

CSystem g_System;
namespace vgui2 {
ISystem* g_pSystem = &g_System;
}
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSystem, ISystem, "VGUI_System009", g_System);

CSystem::CSystem() {
    if (!cse_vgui2_clock::Initialize(m_clock, cse_vgui2_clock::NanosecondsPerSecond)) {
        std::abort();
    }
    const int count = *_NSGetArgc();
    char** arguments = *_NSGetArgv();
    for (int index = 0; index < count; ++index) {
        if (index) {
            m_commandLine += ' ';
        }
        m_commandLine += '"';
        for (const char* character = arguments[index]; *character; ++character) {
            if (*character == '"' || *character == '\\') {
                m_commandLine += '\\';
            }
            m_commandLine += *character;
        }
        m_commandLine += '"';
    }
    if (const char* home = std::getenv("HOME")) {
        m_desktop = std::string(home) + "/Desktop";
    }
}

CSystem::~CSystem() {
    Shutdown();
}

void CSystem::Shutdown() {
    if (m_pUserConfigData) {
        m_pUserConfigData->deleteThis();
        m_pUserConfigData = nullptr;
    }
}

void CSystem::RunFrame() {
    m_flFrameTime = GetCurrentTime();
    if (m_bStaticWatchForComputerUse && g_pInput) {
        int x = 0;
        int y = 0;
        g_pInput->GetCursorPos(x, y);
        constexpr long long MovementThreshold = 50;
        const long long movement = static_cast<long long>(x) + y - m_iStaticMouseOldX - m_iStaticMouseOldY;
        if (std::llabs(movement) > MovementThreshold) {
            m_StaticLastComputerUseTime = m_flFrameTime;
            m_iStaticMouseOldX = x;
            m_iStaticMouseOldY = y;
        }
    }
}

double CSystem::GetFrameTime() {
    return m_flFrameTime;
}

double CSystem::GetCurrentTime() {
    std::lock_guard<std::mutex> guard(m_TimeLock);
    std::uint64_t counter = 0;
    double result = 0;
    if (!cse_vgui2_clock::ReadMonotonicCounter(counter) || !cse_vgui2_clock::Advance(m_clock, counter, result)) {
        std::fputs("VGUI2 monotonic clock failed\n", stderr);
        std::abort();
    }
    return result;
}

std::int32_t CSystem::GetTimeMillis() {
    return cse_vgui2_clock::Milliseconds(GetCurrentTime());
}

void CSystem::ShellExecute(const char*, const char* file) {
    if (!file || !*file) {
        return;
    }
    char executable[] = "/usr/bin/open";
    char separator[] = "--";
    char* arguments[]{executable, separator, const_cast<char*>(file), nullptr};
    pid_t process = 0;
    if (posix_spawn(&process, executable, nullptr, nullptr, arguments, *_NSGetEnviron()) == 0) {
        int status = 0;
        while (waitpid(process, &status, 0) < 0 && errno == EINTR) {
        }
    }
}

void CSystem::SetClipboardText(const char* text, int length) {
    cse::ui_clipboard::Write(text, length);
}

void CSystem::SetClipboardText(const wchar_t* text, int length) {
    cse::ui_clipboard::Write(text, length);
}

int CSystem::GetClipboardTextCount() {
    return cse::ui_clipboard::Count();
}

namespace {
std::vector<unsigned char> ClipboardUnicode() {
    const auto cells = cse::ui_clipboard::ReadCells();
    if (cells.empty()) {
        return {};
    }
    constexpr std::size_t Utf16Bytes = 2;
    std::vector<unsigned char> bytes(cells.size() * Utf16Bytes);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const auto unit = static_cast<std::uint16_t>(cells[index]);
        bytes[index * Utf16Bytes] = static_cast<unsigned char>(unit);
        bytes[index * Utf16Bytes + 1] = static_cast<unsigned char>(unit >> 8);
    }
    return bytes;
}
}

int CSystem::GetClipboardText(int offset, char* destination, int capacity) {
    if (capacity <= 0) {
        return 0;
    }
    const auto bytes = ClipboardUnicode();
    const cse_vgui2_clipboard::SnapshotView snapshot{0, false, 0, !bytes.empty(),
        {bytes.data(), bytes.size()}, bytes.size()};
    std::int32_t copied = 0;
    cse_vgui2_clipboard::ReadUnicodeBytes(snapshot, offset,
        {reinterpret_cast<unsigned char*>(destination), static_cast<std::uint64_t>(capacity)}, copied);
    return copied;
}

int CSystem::GetClipboardText(int offset, wchar_t* destination, int capacity) {
    if (capacity <= 0) {
        return 0;
    }
    const auto bytes = ClipboardUnicode();
    const cse_vgui2_clipboard::SnapshotView snapshot{0, false, 0, !bytes.empty(),
        {bytes.data(), bytes.size()}, bytes.size()};
    std::int32_t copied = 0;
    cse_vgui2_clipboard::ReadUnicodeCells(snapshot, offset,
        {destination, static_cast<std::uint64_t>(capacity)}, copied);
    return copied;
}

bool CSystem::SetRegistryString(const char* key, const char* value) {
    return cse::mac_settings::WriteString(key, value);
}

bool CSystem::GetRegistryString(const char* key, char* value, int capacity) {
    return capacity >= 0 && cse::mac_settings::ReadBytes(key, value, static_cast<std::size_t>(capacity));
}

bool CSystem::SetRegistryInteger(const char* key, int value) {
    return cse::mac_settings::WriteInt32(key, value);
}

bool CSystem::GetRegistryInteger(const char* key, int& value) {
    return cse::mac_settings::ReadInt32(key, value);
}

bool CSystem::DeleteRegistryKey(const char*) {
    return false;
}

bool CSystem::SetWatchForComputerUse(bool state) {
    m_bStaticWatchForComputerUse = state;
    m_StaticLastComputerUseTime = GetCurrentTime();
    return true;
}

double CSystem::GetTimeSinceLastUse() {
    return m_bStaticWatchForComputerUse ? GetCurrentTime() - m_StaticLastComputerUseTime : 0;
}

int CSystem::GetAvailableDrives(char* destination, int capacity) {
    constexpr char RootDrive[] = "/\0";
    if (!destination || capacity < static_cast<int>(sizeof(RootDrive))) {
        return 0;
    }
    std::memcpy(destination, RootDrive, sizeof(RootDrive));
    return sizeof(RootDrive) - 1;
}

double CSystem::GetFreeDiskSpace(const char* path) {
    struct statvfs information{};
    return path && statvfs(path, &information) == 0
        ? static_cast<double>(information.f_bavail) * static_cast<double>(information.f_frsize) : 0;
}

bool CSystem::CommandLineParamExists(const char* name) {
    if (!name) {
        return false;
    }
    char** arguments = *_NSGetArgv();
    for (int index = 1; index < *_NSGetArgc(); ++index) {
        if (std::strcmp(arguments[index], name) == 0) {
            return true;
        }
    }
    return false;
}

bool CSystem::GetCommandLineParamValue(const char* name, char* destination, int capacity) {
    if (!name || !destination || capacity <= 0) {
        return false;
    }
    char** arguments = *_NSGetArgv();
    for (int index = 1; index + 1 < *_NSGetArgc(); ++index) {
        if (std::strcmp(arguments[index], name) == 0) {
            const auto length = std::strlen(arguments[index + 1]);
            if (length >= static_cast<std::size_t>(capacity)) {
                return false;
            }
            std::memcpy(destination, arguments[index + 1], length + 1);
            return true;
        }
    }
    return false;
}

const char* CSystem::GetFullCommandLine() {
    return m_commandLine.c_str();
}

bool CSystem::GetCurrentTimeAndDate(int* year, int* month, int* weekDay, int* day,
    int* hour, int* minute, int* second) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (!localtime_r(&now, &local)) {
        return false;
    }
    constexpr int CalendarYearOffset = 1900;
    if (year) {
        *year = local.tm_year + CalendarYearOffset;
    }
    if (month) {
        *month = local.tm_mon + 1;
    }
    if (weekDay) {
        *weekDay = local.tm_wday;
    }
    if (day) {
        *day = local.tm_mday;
    }
    if (hour) {
        *hour = local.tm_hour;
    }
    if (minute) {
        *minute = local.tm_min;
    }
    if (second) {
        *second = local.tm_sec;
    }
    return true;
}

bool CSystem::CreateShortcut(const char*, const char*, const char*, const char*, const char*) {
    return false;
}

bool CSystem::GetShortcutTarget(const char*, char*, char*, int) {
    return false;
}

bool CSystem::ModifyShortcutTarget(const char*, const char*, const char*, const char*) {
    return false;
}

const char* CSystem::GetDesktopFolderPath() {
    return m_desktop.c_str();
}

KeyCode CSystem::KeyCode_VirtualKeyToVGUI(int key) {
    return ::KeyCode_VirtualKeyToVGUI(key);
}

int CSystem::KeyCode_VGUIToVirtualKey(KeyCode key) {
    return ::KeyCode_VGUIToVirtualKey(key);
}

KeyValues* CSystem::GetUserConfigFileData( const char* dialogName, int dialogID ) {
    constexpr std::size_t MaximumDialogKeyBytes = 256;
    char buf[MaximumDialogKeyBytes];

    if ( !m_pUserConfigData ) {
        return NULL;
    }

    if ( dialogID ) {
        snprintf( buf, sizeof( buf ), "%s_%d", dialogName, dialogID );
        dialogName = buf;
    }

    return m_pUserConfigData->FindKey( dialogName, true );
}

void CSystem::SetUserConfigFile( const char* fileName, const char* pathName ) {
    if ( m_pUserConfigData ) {
        m_pUserConfigData->Clear();
    }
    else {
        m_pUserConfigData = new KeyValues( "UserConfigData" );
    }

    std::snprintf(m_szFileName, sizeof(m_szFileName), "%s", fileName ? fileName : "");
    std::snprintf(m_szPathID, sizeof(m_szPathID), "%s", pathName ? pathName : "");
    m_pUserConfigData->LoadFromFile( g_pFileSystem, m_szFileName, m_szPathID );
}

void CSystem::SaveUserConfigFile() {
    if ( m_pUserConfigData ) {
        m_pUserConfigData->SaveToFile( g_pFileSystem, m_szFileName, m_szPathID );
    }
}

