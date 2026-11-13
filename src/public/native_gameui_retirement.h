#pragma once

#include "cse_module_abi.h"
#include "interface_registry.h"
#include <cstdint>

namespace cse::gameui_lifetime {
constexpr uint32_t ContractVersion = 1;
constexpr char HostInterface[] = "CSEGameUIHost001";
constexpr char RetirementInterface[] = "CSEGameUIRetirement001";

struct HostV1 {
    uint32_t structBytes;
    uint32_t version;
    bool(CSE_ABI_CALL *IsMainThread)();
    bool(CSE_ABI_CALL *IsRetirementSafePoint)();
};

struct RetirementV1 {
    uint32_t structBytes;
    uint32_t version;
    bool(CSE_ABI_CALL *TryRetire)();
    bool(CSE_ABI_CALL *IsRetired)();
};

inline const HostV1 *AdmitHost(CreateInterfaceFn factory) {
    const auto *host = factory ? static_cast<const HostV1 *>(factory(HostInterface, nullptr)) : nullptr;
    return host && host->structBytes == sizeof(HostV1) && host->version == ContractVersion && host->IsMainThread &&
                   host->IsRetirementSafePoint
               ? host
               : nullptr;
}

inline const RetirementV1 *AdmitRetirement(CreateInterfaceFn factory) {
    const auto *owner = factory ? static_cast<const RetirementV1 *>(factory(RetirementInterface, nullptr)) : nullptr;
    return owner && owner->structBytes == sizeof(RetirementV1) && owner->version == ContractVersion &&
                   owner->TryRetire && owner->IsRetired
               ? owner
               : nullptr;
}
}
