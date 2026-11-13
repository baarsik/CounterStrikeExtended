#ifndef CSE_LOCALIZATION_FONT_BACKEND_H
#define CSE_LOCALIZATION_FONT_BACKEND_H

#include "FontManager.h"
#include "../../public/localization_service.h"

namespace cse {
namespace localization {
namespace font_backend_detail {
inline bool CSE_ABI_CALL Begin(const char* language) {
    return FontManager().BeginLanguageFonts(language);
}
inline bool CSE_ABI_CALL Stage(std::uint32_t font, const char* name, int tall, int weight,
    int blur, int scanlines, int flags) {
    return FontManager().StageLanguageFont(font, name, tall, weight, blur, scanlines, flags);
}
inline void CSE_ABI_CALL Finish() {
    FontManager().FinishLanguageFonts();
}
inline void CSE_ABI_CALL Commit() {
    FontManager().CommitLanguageFonts();
}
inline void CSE_ABI_CALL Discard() {
    FontManager().DiscardLanguageFonts();
}
} // namespace font_backend_detail

inline const FontBackend* LocalFontBackend() {
    static const FontBackend backend{sizeof(FontBackend), InterfaceVersion,
        font_backend_detail::Begin, font_backend_detail::Stage, font_backend_detail::Finish,
        font_backend_detail::Commit, font_backend_detail::Discard};
    return &backend;
}
} // namespace localization
} // namespace cse

#endif
