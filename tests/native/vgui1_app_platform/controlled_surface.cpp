#include "VGUI_SurfaceBase.h"
#include <cstdlib>

namespace vgui {
// Controlled real-type construction only: the retained production constructor
// creates Panel/ImagePanel UI and is intentionally never linked or executed.
SurfaceBase::SurfaceBase(Panel*) : _needsSwap(false), _app(nullptr), _embeddedPanel(nullptr),
    _emulatedCursor(nullptr), _currentCursor(nullptr) {
}

template<class Tag, class Type, Type Member> struct Access {
    friend auto ModeData(Tag) {
        return Member;
    }
};
struct ModeTag { friend auto ModeData(ModeTag); };
using ModeDar = Dar<char*>;
template struct Access<ModeTag, decltype(&ModeDar::_data), &ModeDar::_data>;
SurfaceBase::~SurfaceBase() {
    delete[] (_modeInfoDar.*ModeData(ModeTag{}));
}
Panel* SurfaceBase::getPanel() {
    std::abort();
}
void SurfaceBase::requestSwap() {
    std::abort();
}
void SurfaceBase::resetModeInfo() {
    std::abort();
}
int SurfaceBase::getModeInfoCount() {
    std::abort();
}
bool SurfaceBase::getModeInfo(int, int&, int&, int&) {
    std::abort();
}
App* SurfaceBase::getApp() {
    std::abort();
}
void SurfaceBase::setEmulatedCursorVisible(bool) {
    std::abort();
}
void SurfaceBase::setEmulatedCursorPos(int, int) {
    std::abort();
}
void SurfaceBase::addModeInfo(int, int, int) {
    std::abort();
}
}
