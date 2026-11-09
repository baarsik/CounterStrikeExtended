// Link-only guards. Any accidental native HAL execution fails the fixture.
#include <CoreAudio/CoreAudio.h>
#include <cstdlib>
extern "C" Boolean AudioObjectHasProperty(AudioObjectID, const AudioObjectPropertyAddress*) {
    std::abort();
}
extern "C" OSStatus AudioObjectIsPropertySettable(AudioObjectID, const AudioObjectPropertyAddress*, Boolean*) {
    std::abort();
}
extern "C" OSStatus AudioObjectGetPropertyDataSize(AudioObjectID, const AudioObjectPropertyAddress*, UInt32,
    const void*, UInt32*) {
    std::abort();
}
extern "C" OSStatus AudioObjectGetPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32,
    const void*, UInt32*, void*) {
    std::abort();
}
extern "C" OSStatus AudioObjectSetPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32,
    const void*, UInt32, const void*) {
    std::abort();
}
