#ifndef CSE_TEST_FAKE_FONT_PROVIDER_H
#define CSE_TEST_FAKE_FONT_PROVIDER_H
#include "VGui1FontPlatform.h"

struct FakeFontStats {
    int contextsCreated;
    int contextsDestroyed;
    int facesCreated;
    int facesDestroyed;
    int liveFaces;
    int registrations;
    int releases;
    int realizations;
};
FakeFontStats FakeFontSnapshot();
void FakeFontFailRegister(int call);
void FakeFontFailRelease(int count);
void FakeFontDropCapabilities(std::uint32_t capabilities);
void FakeFontFailCreate(bool fail);
void FakeFontInvalidMetrics(bool invalid);
cse_vgui1::FontRequest FakeFontLastRequest();
unsigned int FakeFontLastByte();
int FakeFontLastReleaseLiveFaces();
extern "C" FakeFontStats FakeFontSnapshotC();
extern "C" void FakeFontFailReleaseC(int count);

namespace vgui {
class SurfacePlat {
public:
    BaseFontPlat* selected = nullptr;
};
}
#endif
