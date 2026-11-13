#pragma once

#include <cstdint>

#if INTPTR_MAX > INT32_MAX
#define CSE_NATIVE_SYSTEM_MODULE 1
#include "../common/native/IBaseSystem.h"
#include "cse_module_abi.h"
#include <cstring>

namespace cse {
namespace modules {

constexpr char FactoryName[] = "CSESystemModuleFactory001";
constexpr char ConsumerName[] = "CSESystemModuleConsumer001";
constexpr char ModuleName[] = "ISystemModule001";
constexpr char SystemName[] = "IBaseSystem001";
constexpr char EngineName[] = "enginewrapper002";
constexpr char DemoName[] = "demoplayer001";
constexpr char ServerName[] = "server001";
constexpr char WorldName[] = "world001";
constexpr char NetworkName[] = "network001";
constexpr char ProxyName[] = "proxy001";
constexpr char DirectorName[] = "director001";
constexpr char LifetimeName[] = "CSEModuleLifetime001";
constexpr char SubscriptionName[] = "CSEModuleSubscription001";
constexpr std::uint32_t ContractRevision = 1;
// Signature describes the source-owned virtual-base/query contract, not sizeof(C++ interface).
constexpr std::uint32_t ContractFamily = 0x53594d31; // SYM1
constexpr auto Contract =
    abi::Describe({{ContractFamily, 0,
                    abi::LayoutHash({ContractRevision, 1 /* shared virtual base */, 1 /* provider query */,
                                     1 /* provider release */, 1 /* canonical frame */, 1 /* staged lifetime */})}});

struct Factory {
    abi::Descriptor descriptor;
    std::uint32_t bytes;
    std::uint32_t revision;
    ISystemModule *(CSE_ABI_CALL *Create)(const char *name);
    void(CSE_ABI_CALL *Release)(ISystemModule *module);
};

inline bool Is(const char *name, const char *expected) {
    return name && std::strcmp(name, expected) == 0;
}

inline const Factory *Admit(CreateInterfaceFn factory) {
    if (!factory) {
        return nullptr;
    }
    const auto *api = static_cast<const Factory *>(factory(FactoryName, nullptr));
    if (!api || abi::CheckRequired(Contract, &api->descriptor) != abi::Compatibility::Compatible ||
        api->bytes != sizeof(Factory) || api->revision != ContractRevision || !api->Create || !api->Release) {
        return nullptr;
    }
    return api;
}

inline bool AdmitConsumer(CreateInterfaceFn factory) {
    return factory &&
           abi::CheckRequired(Contract, static_cast<const abi::Descriptor *>(factory(ConsumerName, nullptr))) ==
               abi::Compatibility::Compatible;
}

template <class T> T *Query(ISystemModule *module, const char *name) {
    return module ? static_cast<T *>(module->QueryInterface(name)) : nullptr;
}

template <class T>
T *Acquire(IBaseSystem *system, const char *name, const char *library, const char *instance = nullptr) {
    return system ? Query<T>(system->GetModule(const_cast<char *>(name), const_cast<char *>(library),
                                               const_cast<char *>(instance)),
                             name)
                  : nullptr;
}

// A borrowed control table lets callbacks in other images retain the owning registry.
struct Lifetime {
    unsigned int depth = 0;
};

class CallbackScope {
public:
    explicit CallbackScope(Lifetime *lifetime) : m_Lifetime(lifetime) {
        if (m_Lifetime) {
            ++m_Lifetime->depth;
        }
    }
    explicit CallbackScope(IBaseSystem *system) : CallbackScope(Query<Lifetime>(system, LifetimeName)) {
    }
    ~CallbackScope() {
        if (m_Lifetime) {
            --m_Lifetime->depth;
        }
    }
    CallbackScope(const CallbackScope &) = delete;
    CallbackScope &operator=(const CallbackScope &) = delete;

private:
    Lifetime *m_Lifetime;
};

struct Subscription {
    void *context;
    bool (*Register)(void *context, ISystemModule *listener);
};

inline bool Subscribe(ISystemModule *publisher, ISystemModule *listener) {
    auto *subscription = Query<Subscription>(publisher, SubscriptionName);
    return subscription && subscription->Register && subscription->Register(subscription->context, listener);
}

} // namespace modules
} // namespace cse
#else
#define CSE_NATIVE_SYSTEM_MODULE 0
#endif
