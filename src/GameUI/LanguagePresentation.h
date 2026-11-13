#pragma once

#include <cstdint>

namespace vgui2 { class Panel; }

namespace cse::gameui {
void RefreshLanguagePresentation(vgui2::Panel* root, std::uint64_t revision);
}
