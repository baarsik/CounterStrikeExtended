#include "VGui1HostImageOwner.h"

extern "C" __attribute__((visibility("default"))) int MisplacedOwnerProbe(
    const cse_vgui1_host::AdmittedImageV1* image) {
    cse_vgui1_host::ImageOwner owner;
    return static_cast<int>(owner.Open(*image, cse_vgui1_host::AnchorMode::FirstTransferredReference, nullptr));
}
