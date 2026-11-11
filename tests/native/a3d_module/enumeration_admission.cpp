#include "enumeration_caller.h"
#include "cse_a3d_abi.h"
#include <array>
#include <type_traits>

namespace {
using namespace cse::a3d_abi;
using independent::DeviceInfo;
using independent::Enumerator;
constexpr auto kBase = Describe<IA3dSource202, IA3dGeomTarget, IA3dListenerTarget>();
static_assert(kBase.tableCount == 16 && sizeof(kBase) == 288);
static_assert(sizeof(GUID) == 16 && alignof(GUID) == 4);
static_assert(offsetof(GUID, Data1) == 0 && sizeof(GUID::Data1) == 4);
static_assert(offsetof(GUID, Data2) == 4 && sizeof(GUID::Data2) == 2);
static_assert(offsetof(GUID, Data3) == 6 && sizeof(GUID::Data3) == 2);
static_assert(offsetof(GUID, Data4) == 8 && sizeof(GUID::Data4) == 8);
static_assert(sizeof(HRESULT) == 4 && std::is_signed_v<HRESULT>);
static_assert(sizeof(DWORD) == 4 && std::is_unsigned_v<DWORD>);
static_assert(sizeof(ULONG) == 4 && std::is_unsigned_v<ULONG> && sizeof(char) == 1);
static_assert(std::is_standard_layout_v<DeviceInfo> && sizeof(DeviceInfo) == 32 && alignof(DeviceInfo) == 4);
static_assert(!std::is_same_v<DeviceInfo, A3dNativeDeviceInfoV1>);
static_assert(!std::is_same_v<Enumerator, IA3dNativeDeviceEnumeratorV1>);
static_assert(!std::has_virtual_destructor_v<Enumerator>);
#define COMPARE_FIELD(field, offset, bytes) \
    static_assert(offsetof(DeviceInfo, field) == offset && sizeof(decltype(DeviceInfo::field)) == bytes && \
        alignof(decltype(DeviceInfo::field)) == 4); \
    static_assert(offsetof(DeviceInfo, field) == offsetof(A3dNativeDeviceInfoV1, field) && \
        Type<decltype(DeviceInfo::field)>() == Type<decltype(A3dNativeDeviceInfoV1::field)>())
COMPARE_FIELD(structureBytes, 0, 4);
COMPARE_FIELD(version, 4, 4);
COMPARE_FIELD(device, 8, 16);
COMPARE_FIELD(nameBytes, 24, 4);
COMPARE_FIELD(reserved, 28, 4);
#undef COMPARE_FIELD
static_assert(Guid(independent::EnumeratorIid) == Guid(IID_IA3dNativeDeviceEnumeratorV1));
static_assert(independent::Version == A3D_NATIVE_DEVICE_ENUMERATION_VERSION);
static_assert(RecordsFor<DeviceInfo>() == Records);
static_assert(Function<independent::Factory>::Signature == Function<CreateNativeDeviceEnumerator>::Signature);
#define COMPARE_METHOD(method) \
    static_assert(Method<decltype(&Enumerator::method)>::Signature == \
        Method<decltype(&IA3dNativeDeviceEnumeratorV1::method)>::Signature)
COMPARE_METHOD(QueryInterface);
COMPARE_METHOD(AddRef);
COMPARE_METHOD(Release);
COMPARE_METHOD(GetCount);
COMPARE_METHOD(GetInfo);
#undef COMPARE_METHOD

constexpr auto kUnknown = Method<decltype(&Enumerator::QueryInterface)>::Signature;
constexpr auto kAddRef = Method<decltype(&Enumerator::AddRef)>::Signature;
constexpr auto kRelease = Method<decltype(&Enumerator::Release)>::Signature;
constexpr auto kCount = Method<decltype(&Enumerator::GetCount)>::Signature;
constexpr auto kInfo = Method<decltype(&Enumerator::GetInfo)>::Signature;
constexpr auto kSlots = Slots({kUnknown, kAddRef, kRelease, kCount, kInfo});
constexpr auto kDeviceSlots = Slots({kUnknown, kAddRef, kRelease,
    Method<decltype(&independent::Device::InitializeDevice)>::Signature});
constexpr auto kCapacitySlots = Slots({kUnknown, kAddRef, kRelease,
    Method<decltype(&IA3dNativeCapacityV1::GetVoiceCapacity)>::Signature});

constexpr std::uint64_t PrivateFacts(std::uint64_t slots = kSlots,
    const GUID& iid = independent::EnumeratorIid, DWORD version = 1, DWORD recordBytes = 32, DWORD queryPolicy = 1) {
    return LayoutHash({kDeviceSlots, kCapacitySlots, Guid(independent::DeviceIid), Guid(IID_IA3dNativeCapacityV1),
        slots, Guid(iid), version, recordBytes, queryPolicy});
}
constexpr std::uint64_t ExportFacts(std::uint64_t signature = Function<independent::Factory>::Signature,
    std::uint64_t name = NameFacts(independent::FactoryName)) {
    return LayoutHash({Function<GetClassObject>::Signature, Function<CanUnloadNow>::Signature,
        Function<CreateNativeEvent>::Signature, Function<cse::abi::GetModuleAbi>::Signature, signature, name});
}
constexpr std::uint64_t IdentityFacts(const GUID& iid = independent::EnumeratorIid) {
    return LayoutHash({Guid(CLSID_A3dApi), Guid(CLSID_A3d), Guid(IID_IUnknown),
        Guid(IID_IClassFactory), Guid(IID_IA3d), Guid(IID_IA3d2), Guid(IID_IA3d3),
        Guid(IID_IA3dSource), Guid(IID_IA3dGeom), Guid(IID_IA3dListener), Guid(IID_IA3dMaterial),
        Guid(IID_IA3dList), Guid(IID_IDirectSound), Guid(IID_IDirectSoundBuffer),
        Guid(IID_IDirectSound3DBuffer), Guid(IID_IDirectSoundNotify), Guid(iid)});
}
constexpr std::uint64_t ProfileFacts(DWORD profile = 1) {
    return LayoutHash({Revision, CallProfile, 1, 1, profile});
}
void Replace(cse::abi::Descriptor& descriptor, DWORD identifier, std::uint64_t signature) {
    for (auto& table : descriptor.tables) {
        if (table.identifier == identifier) {
            table.signature = signature;
            return;
        }
    }
}

struct WideRecord {
    DWORD structureBytes;
    std::uint64_t version;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
struct ReorderedRecord {
    DWORD version;
    DWORD structureBytes;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
struct alignas(8) AlignedRecord {
    DWORD structureBytes;
    DWORD version;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
struct SignedRecord {
    DWORD structureBytes;
    DWORD version;
    GUID device;
    std::int32_t nameBytes;
    DWORD reserved;
};
struct WideMethods {
    HRESULT GetInfo(std::uint64_t, DWORD, DWORD, DeviceInfo*, char*, DWORD, DWORD*);
};
using WideFactory = HRESULT (*)(Enumerator**, std::uint64_t);
}

cse::abi::Descriptor ExpectedA3dEnumerationDescriptor() {
    auto expected = kBase;
    Replace(expected, ProviderRole, ProfileFacts());
    Replace(expected, RecordContract, RecordsFor<DeviceInfo>());
    Replace(expected, PrivateDeviceContract, PrivateFacts());
    Replace(expected, ExportContract, ExportFacts());
    Replace(expected, IdentityContract, IdentityFacts());
    return expected;
}

bool CheckEnumerationAdmissionMutations(const cse::abi::Descriptor& offered) {
    const auto expected = ExpectedA3dEnumerationDescriptor();
    const auto rejects = [&](const cse::abi::Descriptor* candidate) {
        return cse::abi::Check(expected, candidate) != cse::abi::Compatibility::Compatible;
    };
    if (rejects(&offered) || !rejects(nullptr)) {
        return false;
    }
    // Every common field and every table field must be admitted before calling code.
    for (unsigned field = 0; field < 8; ++field) {
        auto changed = offered;
        std::uint32_t* fields[] = {&changed.descriptorBytes, &changed.descriptorVersion,
            &changed.runtimeVersion, &changed.architecture, &changed.pointerBits, &changed.compilerAbi,
            &changed.tableCount, &changed.reserved};
        ++*fields[field];
        if (!rejects(&changed)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < offered.tableCount; ++index) {
        for (unsigned field = 0; field < 3; ++field) {
            auto changed = offered;
            if (field == 0) {
                ++changed.tables[index].identifier;
            } else if (field == 1) {
                ++changed.tables[index].reserved;
            } else {
                ++changed.tables[index].signature;
            }
            if (!rejects(&changed)) {
                return false;
            }
        }
    }
    auto changedIid = independent::EnumeratorIid;
    ++changedIid.Data1;
    const std::array<std::pair<DWORD, std::uint64_t>, 14> semanticChanges{{
        {RecordContract, RecordsFor<WideRecord>()},
        {RecordContract, RecordsFor<ReorderedRecord>()},
        {RecordContract, RecordsFor<AlignedRecord>()},
        {RecordContract, RecordsFor<SignedRecord>()},
        {PrivateDeviceContract, PrivateFacts(Slots({kUnknown, kAddRef, kRelease, kInfo, kCount}))},
        {PrivateDeviceContract, PrivateFacts(Slots({kUnknown, kAddRef, kRelease, kCount,
            Method<decltype(&WideMethods::GetInfo)>::Signature}))},
        {PrivateDeviceContract, PrivateFacts(kSlots, changedIid)},
        {PrivateDeviceContract, PrivateFacts(kSlots, independent::EnumeratorIid, 2)},
        {PrivateDeviceContract, PrivateFacts(kSlots, independent::EnumeratorIid, 1, 33)},
        {PrivateDeviceContract, PrivateFacts(kSlots, independent::EnumeratorIid, 1, 32, 2)},
        {ExportContract, ExportFacts(Function<WideFactory>::Signature)},
        {ExportContract, ExportFacts(Function<independent::Factory>::Signature,
            NameFacts("CseCreateA3dNativeDeviceEnumeratorV2"))},
        {IdentityContract, IdentityFacts(changedIid)},
        {ProviderRole, ProfileFacts(2)}
    }};
    for (const auto& change : semanticChanges) {
        auto changedExpected = expected;
        Replace(changedExpected, change.first, change.second);
        if (cse::abi::Check(changedExpected, &offered) == cse::abi::Compatibility::Compatible) {
            return false;
        }
    }
    return true;
}
