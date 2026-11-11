#ifndef CSE_SERVER_BROWSER_CONFIG_H
#define CSE_SERVER_BROWSER_CONFIG_H

#include "ServerBrowserModel.h"
class IFileSystem;
namespace cse::server_browser {
enum class ReadResult { Missing, Ok, Error };
class IConfigStorage {
public:
    virtual ~IConfigStorage() = default;
    virtual ReadResult Read(std::string& output, std::string& error) = 0;
    virtual bool AtomicReplace(std::string_view bytes, std::string& error) = 0;
};
struct BrowserSettings {
    ListKind kind = ListKind::Internet;
    Filters filters;
    Sort sort;
    int width = 640;
    int height = 480;
};
struct ConfigStatus {
    bool available = false;
    bool dirty = false;
    std::string error;
};
class ServerBrowserConfig final {
public:
    explicit ServerBrowserConfig(IConfigStorage& storage);
    ~ServerBrowserConfig();
    bool Load();
    const BrowserSettings& Settings() const;
    void SetSettings(const BrowserSettings& settings);
    bool Save();
    ConfigStatus Status() const;
private:
    struct State;
    std::unique_ptr<State> state_;
};
// Explicitly constructed only; empty preferenceRoot resolves configuration-specific SDL preferences.
// The borrowed filesystem must outlive this object. nullptr supports headless injected-root tools.
class NativeConfigStorage final : public IConfigStorage {
public:
    explicit NativeConfigStorage(IFileSystem* filesystem, std::string preferenceRoot = {});
    ~NativeConfigStorage() override;
    bool Available() const;
    const std::string& Directory() const;
    ReadResult Read(std::string& output, std::string& error) override;
    bool AtomicReplace(std::string_view bytes, std::string& error) override;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
#endif
