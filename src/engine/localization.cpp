#include "localization.h"
#include "localization_state.h"
#include "filesystem.h"
#include "cmd.h"
#include "conprint.h"
#include "tmessage.h"
#include "vgui2/BaseUISurface.h"
#include "../vgui2/vgui_surfacelib/FontManager.h"
#include "../vgui2/vgui_surfacelib/LocalizationFontBackend.h"
#include "../common/interface_language_preference.h"
#include "../public/localization_service.h"

#include <cstring>

bool SCR_ReplaceCenterText(const char* expectedText, const char* replacementText);

namespace cse {
namespace localization {
namespace {
State state;
bool initialized = false;
const RuntimeApi* runtime = nullptr;

void CSE_ABI_CALL ResetEngineFont(std::uint32_t font) {
    FontManager().ResetFont(static_cast<vgui2::HFont>(font));
}

class EngineBackend final : public StateBackend {
public:
    std::string Resolve(const std::string& preference) override {
        return FileSystem_ResolveInterfaceLanguage(preference.c_str());
    }

    bool Prepare(const std::string& language, std::string& error) override {
        if (!runtime) {
            error = "The interface localization provider is not ready.";
            return false;
        }
        IFileSystem* view = FileSystem_PrepareInterfaceLanguage(language.c_str());
        if (!view) {
            error = "The localized resource search paths could not be prepared.";
            return false;
        }
        if (!TextMessagePrepareLanguage(view, FileSystem_InterfaceLanguageBaseline())) {
            error = "The interface title catalogue is missing or invalid.";
            return false;
        }
        if (!runtime->PrepareLanguage(view, language.c_str())) {
            error = "The interface translation catalogue or fonts could not be prepared.";
            return false;
        }
        return true;
    }

    bool Persist(const std::string& preference, std::string& error) override {
        if (!interface_language::WriteSavedPreference(preference.c_str())) {
            error = "The interface language preference could not be saved.";
            return false;
        }
        return true;
    }

    void Commit(const std::string& language, std::uint64_t revision) override {
        FileSystem_CommitInterfaceLanguage();
        g_BaseUISurface.SetLanguage(language.c_str());
        TextMessageCommitLanguage();
        runtime->CommitLanguage(revision);
    }

    void Discard() override {
        if (runtime) {
            runtime->DiscardPrepared();
        }
        TextMessageDiscardLanguage();
        FileSystem_DiscardInterfaceLanguage();
    }
};

const char* Preference() {
    Initialize();
    return state.Preference().c_str();
}

std::uint64_t Revision() {
    Initialize();
    return state.Revision();
}

std::uint64_t RequestLanguage(const char* preference, bool persist) {
    Initialize();
    return state.Request(preference, persist);
}

bool GetRequestResult(std::uint64_t request, RequestResult* result) {
    return state.Result(request, result);
}

const char* LastError() {
    return state.LastError();
}

const EngineApi api{sizeof(EngineApi), InterfaceVersion, Language, Preference, Revision, RequestLanguage,
    GetRequestResult, LastError, TextMessageIsCatalogue, SCR_ReplaceCenterText};

void* Factory() {
    return const_cast<EngineApi*>(&api);
}

void LanguageCommand() {
    if (cmd_source != src_command) {
        return;
    }
    if (Cmd_Argc() < 2 || Cmd_Argc() > 3
        || (Cmd_Argc() == 3 && std::strcmp(Cmd_Argv(2), "temporary") != 0)) {
        Con_Printf("cse_language <bundled language|auto> [temporary]\n");
        Con_Printf("Interface language: %s; preference: %s; revision: %llu\n", Language(),
            state.Preference().empty() ? "auto" : state.Preference().c_str(),
            static_cast<unsigned long long>(state.Revision()));
        return;
    }
    const char* requested = Cmd_Argv(1);
    const char* preference = std::strcmp(requested, "auto") == 0 ? "" : requested;
    const auto request = RequestLanguage(preference, Cmd_Argc() != 3);
    if (!request) {
        Con_Printf("Interface language: %s\n", LastError());
    }
}
} // namespace

EXPOSE_INTERFACE_FN(Factory, CSEEngineLocalization, EngineInterface);

void Initialize() {
    if (initialized) {
        return;
    }
    const std::string preference = interface_language::ReadSavedPreference();
    state.Initialize(FileSystem_ResolveInterfaceLanguage(preference.c_str()), preference);
    initialized = true;
}

void InitializeCommand() {
    Cmd_AddCommandWithFlags(const_cast<char*>("cse_language"), LanguageCommand, CMD_FLAG_PRIVILEGED);
}

void Shutdown() {
    UnbindRuntime();
    initialized = false;
    state = State{};
}

void BindRuntime(CreateInterfaceFn factory) {
    runtime = QueryRuntime(factory);
    if (runtime) {
        runtime->BindFontReset(ResetEngineFont);
        runtime->BindFontBackend(LocalFontBackend());
    }
}

void UnbindRuntime() {
    if (runtime) {
        runtime->DiscardPrepared();
        runtime->BindFontReset(nullptr);
        runtime->BindFontBackend(nullptr);
    }
    runtime = nullptr;
    TextMessageDiscardLanguage();
    FileSystem_DiscardInterfaceLanguage();
}

void ProcessFrame() {
    if (initialized) {
        EngineBackend backend;
        state.Process(backend);
    }
}

const char* Language() {
    Initialize();
    return state.Language().c_str();
}

} // namespace localization
} // namespace cse
