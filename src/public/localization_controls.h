#ifndef CSE_PUBLIC_LOCALIZATION_CONTROLS_H
#define CSE_PUBLIC_LOCALIZATION_CONTROLS_H

#include "localization_service.h"
#include "../engine/ILocalize.h"
#include "vgui/KeyValues.h"
#include "../vgui2/controls/TextImage.h"
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vgui2 { ILocalize* localize(); }
namespace cse {
namespace localization {

struct ControlServices {
    const EngineApi* engine = nullptr;
    const RuntimeApi* runtime = nullptr;
};
inline ControlServices& ControlsServices() {
    static ControlServices services;
    return services;
}
inline void BindControls(CreateInterfaceFn* factories, int count) {
    auto& services = ControlsServices();
    services = ControlServices{};
    for (int index = 0; index < count; ++index) {
        if (!services.engine) {
            services.engine = QueryEngine(factories[index]);
        }
        if (!services.runtime) {
            services.runtime = QueryRuntime(factories[index]);
        }
    }
}
inline void ClearControlBindings();
inline void ClearControls() {
    ControlsServices() = ControlServices{};
    ClearControlBindings();
}
inline const EngineApi* ControlsEngine() {
    return ControlsServices().engine;
}
inline std::uint64_t ControlsRevision() {
    const auto* runtime = ControlsServices().runtime;
    const auto* engine = ControlsServices().engine;
    return runtime ? runtime->Revision() : (engine ? engine->Revision() : 0);
}
struct DeleteVariables {
    void operator()(KeyValues* variables) const {
        if (variables) {
            variables->deleteThis();
        }
    }
};
struct TextBinding {
    std::string token;
    std::unique_ptr<KeyValues, DeleteVariables> variables;
    std::uint64_t revision = 0;
    unsigned int updating = 0;
};
inline std::map<const void*, TextBinding>& TextBindings() {
    static std::map<const void*, TextBinding> bindings;
    return bindings;
}
inline TextBinding& Binding(const void* image) {
    return TextBindings()[image];
}
inline void ForgetText(const void* image) {
    TextBindings().erase(image);
}
class PreserveTextBinding {
public:
    explicit PreserveTextBinding(const void* image) : image_(image) {
        ++Binding(image_).updating;
    }

    ~PreserveTextBinding() {
        const auto found = TextBindings().find(image_);
        if (found != TextBindings().end() && found->second.updating) {
            --found->second.updating;
        }
    }

    PreserveTextBinding(const PreserveTextBinding&) = delete;
    PreserveTextBinding& operator=(const PreserveTextBinding&) = delete;

private:
    const void* image_;
};
inline void BindToken(const void* image, const char* text) {
    auto& binding = Binding(image);
    if (binding.updating) {
        return;
    }
    binding.token = text && text[0] == '#' ? text : "";
    binding.variables.reset();
    binding.revision = ControlsRevision();
}
inline void BindLiteral(const void* image) {
    auto& binding = Binding(image);
    if (!binding.updating) {
        binding.token.clear();
        binding.variables.reset();
        binding.revision = ControlsRevision();
    }
}
inline void BindVariables(const void* image, KeyValues* variables) {
    auto& binding = Binding(image);
    if (!binding.token.empty()) {
        binding.variables.reset(variables ? variables->MakeCopy() : nullptr);
        binding.revision = ControlsRevision();
    }
}
inline std::wstring WideLiteral(const char* text) {
    text = text ? text : "";
    const std::size_t length = std::strlen(text);
    constexpr std::size_t MaximumLiteralCells = 32767;
    const std::size_t cells = std::min(length + 1, MaximumLiteralCells);
    std::vector<wchar_t> converted(cells, L'\0');
    vgui2::localize()->ConvertANSIToUnicode(text, converted.data(), static_cast<int>(cells * sizeof(wchar_t)));
    return converted.data();
}
inline bool RefreshText(vgui2::TextImage* image) {
    auto& binding = Binding(image);
    const std::uint64_t revision = ControlsRevision();
    if (binding.token.empty() || binding.updating || binding.revision == revision) {
        return false;
    }
    PreserveTextBinding preserve(image);
    wchar_t* value = vgui2::localize()->Find(binding.token.c_str());
    // Rebind even a previously missing token when a later catalogue adds it.
    image->SetText(binding.token.c_str());
    if (binding.variables && value) {
        // Rendered labels use signed-short storage; leave room for its terminator.
        constexpr int MaximumFormattedCells = 32766;
        std::vector<wchar_t> rendered(MaximumFormattedCells, L'\0');
        vgui2::localize()->ConstructString(rendered.data(),
            static_cast<int>(rendered.size() * sizeof(wchar_t)), binding.token.c_str(), binding.variables.get());
        image->SetText(rendered.data());
    }
    binding.revision = revision;
    return true;
}
inline std::wstring CellText(KeyValues* data, const char* column) {
    if (!data || !column) {
        return {};
    }
    if (data->GetDataType(column) == KeyValues::TYPE_WSTRING) {
        return data->GetWString(column, L"");
    }
    const std::string literalFlag = std::string("__cse_literal_") + column;
    const char* text = data->GetString(column, "");
    if (!data->GetInt(literalFlag.c_str(), 0) && text[0] == '#') {
        if (const wchar_t* translated = vgui2::localize()->Find(text)) {
            return translated;
        }
    }
    return WideLiteral(text);
}
inline void SetCellText(vgui2::TextImage* image, KeyValues* data, const char* column) {
    const std::string literalFlag = std::string("__cse_literal_") + column;
    if (data->GetDataType(column) == KeyValues::TYPE_WSTRING) {
        image->SetText(data->GetWString(column, L""));
    } else if (data->GetInt(literalFlag.c_str(), 0)) {
        image->SetText(WideLiteral(data->GetString(column, "")).c_str());
    } else {
        image->SetText(data->GetString(column, ""));
    }
}
inline std::map<const void*, bool>& NativeCaptions() {
    static std::map<const void*, bool> captions;
    return captions;
}
inline void BindNativeCaption(const void* frame, bool enabled) {
    NativeCaptions()[frame] = enabled;
}
inline bool NativeCaptionEnabled(const void* frame) {
    const auto found = NativeCaptions().find(frame);
    return found != NativeCaptions().end() && found->second;
}
inline std::map<const void*, std::uint64_t>& ControlRevisions() {
    static std::map<const void*, std::uint64_t> revisions;
    return revisions;
}
inline void RememberControl(const void* control) {
    ControlRevisions()[control] = ControlsRevision();
}
inline void ForgetControl(const void* control) {
    ControlRevisions().erase(control);
    NativeCaptions().erase(control);
}
inline bool ControlChanged(const void* control) {
    auto& previous = ControlRevisions()[control];
    const auto revision = ControlsRevision();
    if (previous == revision) {
        return false;
    }
    previous = revision;
    return true;
}

inline void ClearControlBindings() {
    TextBindings().clear();
    ControlRevisions().clear();
    NativeCaptions().clear();
}

} // namespace localization
} // namespace cse
#endif
