#pragma once

#include "cse_module_abi.h"
#include "interface_registry.h"
#include <cstdint>

namespace cse::gameui_connection {
constexpr uint32_t ContractVersion = 1;
constexpr char HostInterface[] = "CSEGameUIConnectionHost001";

// Separate optional interface: the retained GameUI007 and retirement contracts stay unchanged.
struct HostV1 {
    uint32_t structBytes;
    uint32_t version;
    const char*(CSE_ABI_CALL *PreparingAddress)();
};

inline const HostV1* AdmitHost(CreateInterfaceFn factory) {
    const auto* host = factory ? static_cast<const HostV1*>(factory(HostInterface, nullptr)) : nullptr;
    return host && host->structBytes == sizeof(HostV1) && host->version == ContractVersion && host->PreparingAddress
        ? host : nullptr;
}
}
