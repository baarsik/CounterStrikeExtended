#pragma once
#include "vgui/ISystem.h"
#include "check.h"
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>

namespace system_fixture {
// Registry access stays in this fixture. All other calls use the reviewed actual system.
class PrivateRegistry final : public vgui2::ISystem {
public:
    explicit PrivateRegistry(vgui2::ISystem *real) : real_(real) {}
    bool failStringWrites = false;
    int stringWrites = 0;
    std::string languageOverride;
    void Shutdown() override {
        return real_->Shutdown();
    }
    void RunFrame() override {
        return real_->RunFrame();
    }
    void ShellExecute(const char* command, const char* file) override {
        return real_->ShellExecute(command, file);
    }
    double GetFrameTime() override {
        return real_->GetFrameTime();
    }
    double GetCurrentTime() override {
        return real_->GetCurrentTime();
    }
    std::int32_t GetTimeMillis() override {
        return real_->GetTimeMillis();
    }
    int GetClipboardTextCount() override {
        return real_->GetClipboardTextCount();
    }
    void SetClipboardText(const char* text, int textLength) override {
        return real_->SetClipboardText(text, textLength);
    }
    void SetClipboardText(const wchar_t* text, int textLength) override {
        return real_->SetClipboardText(text, textLength);
    }
    int GetClipboardText(int offset, char* buffer, int bufferLength) override {
        return real_->GetClipboardText(offset, buffer, bufferLength);
    }
    int GetClipboardText(int offset, wchar_t* buffer, int bufferLength) override {
        return real_->GetClipboardText(offset, buffer, bufferLength);
    }
    bool SetRegistryString(const char* key, const char* value) override {
        ++stringWrites;
        if (failStringWrites) {
            return false;
        }
        strings_[key] = value;
        return true;
    }
    bool GetRegistryString(const char* key, char* value, int valueLength) override {
        const auto found = strings_.find(key);
        if (found == strings_.end() || valueLength <= 0 ||
            found->second.size() >= static_cast<size_t>(valueLength)) {
            return false;
        }
        std::memcpy(value, found->second.c_str(), found->second.size() + 1);
        return true;
    }
    bool SetRegistryInteger(const char* key, int value) override {
        integers_[key] = value;
        return true;
    }
    bool GetRegistryInteger(const char* key, int& value) override {
        const auto found = integers_.find(key);
        if (found == integers_.end()) {
            return false;
        }
        value = found->second;
        return true;
    }
    KeyValues* GetUserConfigFileData(const char* dialogName, int dialogID) override {
        return real_->GetUserConfigFileData(dialogName, dialogID);
    }
    void SetUserConfigFile(const char* fileName, const char* pathID) override {
        return real_->SetUserConfigFile(fileName, pathID);
    }
    void SaveUserConfigFile() override {
        return real_->SaveUserConfigFile();
    }
    bool SetWatchForComputerUse(bool watch) override {
        return real_->SetWatchForComputerUse(watch);
    }
    double GetTimeSinceLastUse() override {
        return real_->GetTimeSinceLastUse();
    }
    int GetAvailableDrives(char* buffer, int bufferLength) override {
        return real_->GetAvailableDrives(buffer, bufferLength);
    }
    bool CommandLineParamExists(const char* parameter) override {
        if (std::strcmp(parameter, "-language") == 0 && !languageOverride.empty()) {
            return true;
        }
        return real_->CommandLineParamExists(parameter);
    }
    const char* GetFullCommandLine() override {
        return real_->GetFullCommandLine();
    }
    bool GetCurrentTimeAndDate(int* year, int* month, int* dayOfWeek, int* day, int* hour, int* minute,
                               int* second) override {
        return real_->GetCurrentTimeAndDate(year, month, dayOfWeek, day, hour, minute, second);
    }
    double GetFreeDiskSpace(const char* path) override {
        return real_->GetFreeDiskSpace(path);
    }
    bool CreateShortcut(const char* linkFile, const char* targetFile, const char* arguments,
                        const char* workingDirectory, const char* iconFile) override {
        return real_->CreateShortcut(linkFile, targetFile, arguments, workingDirectory, iconFile);
    }
    bool GetShortcutTarget(const char* linkFile, char* targetFile, char* arguments, int bufferLength) override {
        return real_->GetShortcutTarget(linkFile, targetFile, arguments, bufferLength);
    }
    bool ModifyShortcutTarget(const char* linkFile, const char* targetFile, const char* arguments,
                              const char* workingDirectory) override {
        return real_->ModifyShortcutTarget(linkFile, targetFile, arguments, workingDirectory);
    }
    bool GetCommandLineParamValue(const char* parameter, char* value, int valueLength) override {
        if (std::strcmp(parameter, "-language") == 0 && !languageOverride.empty()) {
            if (valueLength <= 0 || languageOverride.size() >= static_cast<size_t>(valueLength)) {
                return false;
            }
            std::memcpy(value, languageOverride.c_str(), languageOverride.size() + 1);
            return true;
        }
        return real_->GetCommandLineParamValue(parameter, value, valueLength);
    }
    bool DeleteRegistryKey(const char* key) override {
        strings_.erase(key);
        integers_.erase(key);
        return true;
    }
    const char* GetDesktopFolderPath() override {
        return real_->GetDesktopFolderPath();
    }
    vgui2::KeyCode KeyCode_VirtualKeyToVGUI(int virtualKey) override {
        return real_->KeyCode_VirtualKeyToVGUI(virtualKey);
    }
    int KeyCode_VGUIToVirtualKey(vgui2::KeyCode key) override {
        return real_->KeyCode_VGUIToVirtualKey(key);
    }
private:
    vgui2::ISystem *real_;
    std::map<std::string, std::string> strings_;
    std::map<std::string, int> integers_;
};
inline PrivateRegistry *current = nullptr;
inline void *Factory(const char *name, int *) {
    return std::strcmp(name, "VGUI_System009") == 0 ? current : nullptr;
}
class BorrowedSystem final {
public:
    BorrowedSystem(void *image, vgui2::ISystem *real) : real_(real), proxy_(real) {
        // The actual image's scheme/localization owners use this global directly.
        global_ = static_cast<vgui2::ISystem **>(dlsym(image, "_ZN5vgui29g_pSystemE"));
        Require(global_ && *global_ == real_, "actual image system global is not the reviewed factory interface");
        *global_ = &proxy_;
        current = &proxy_;
        Require(*global_ == Factory("VGUI_System009", nullptr), "actual image and fixture factory disagree");
    }
    ~BorrowedSystem() {
        Restore();
    }
    void Restore() {
        if (global_) {
            *global_ = real_;
            global_ = nullptr;
            current = nullptr;
        }
    }
private:
    vgui2::ISystem *real_;
    PrivateRegistry proxy_;
    vgui2::ISystem **global_ = nullptr;
};
}
