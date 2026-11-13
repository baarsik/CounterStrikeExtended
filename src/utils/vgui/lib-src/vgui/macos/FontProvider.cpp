#include "../posix/native/VGui1MacFontProvider.h"

namespace cse_vgui1 {

// Select the owned CoreText/FreeType provider for native bring-up. Selection
// establishes no Windows font metric/raster parity or runtime acceptance.
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    return CseVgui1MacFontProviderCandidateV1();
}

} // namespace cse_vgui1
