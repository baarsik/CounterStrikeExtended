#include <cstdint>
#include "vgui/IClientPanel.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/IInput.h"

static_assert(sizeof(vgui2::VPANEL) == sizeof(std::uintptr_t), "VGUI panel identities must retain native pointers");
static_assert(sizeof(vgui2::HPanel) == sizeof(std::uint32_t), "VGUI serial handles retain two 16-bit components");
static_assert(sizeof(vgui2::HScheme) == sizeof(std::uint32_t), "Scheme IDs retain 32-bit storage");
static_assert(sizeof(vgui2::HFont) == sizeof(std::uint32_t), "Font IDs retain 32-bit storage");
static_assert(sizeof(vgui2::HTexture) == sizeof(std::uint32_t), "Texture IDs retain 32-bit storage");
static_assert(sizeof(vgui2::HCursor) == sizeof(std::uint32_t), "Cursor IDs retain 32-bit storage");
int main() {
    return 0;
}
