#include "cse_server_forms.h"
#include "../common/image_codec.h"

#include <utility>

namespace cse::forms {

bool ParseImage(AssetKind kind, std::string_view bytes, Image& image) {
    if (!IsImage(kind) || bytes.size() > kMaxImageBytes) {
        return false;
    }
    if (!IsWebPImage(kind)) {
        return ParseImage(bytes, image);
    }

    cse::image::Image decoded;
    if (!cse::image::DecodeWebP(bytes.data(), bytes.size(), decoded, kMaxImagePixelBytes, kMaxImageSide)) {
        return false;
    }
    image = {decoded.width, decoded.height, std::move(decoded.rgba)};
    return true;
}

} // namespace cse::forms
