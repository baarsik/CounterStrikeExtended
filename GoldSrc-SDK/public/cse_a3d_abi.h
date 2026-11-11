#pragma once
// Include the actual Source, Geometry and Listener declarations at the call site.
// Describe is instantiated independently by producer and consumer. These retained
// views include native failure slots for every SDK source/geometry tail; Windows is unchanged.
#include "cse_a3d_notifications.h"
#include "../a3dapi/platform_dsound.h"
#include "../a3dapi/ia3dapi.h"
#include "../a3dapi/native_device_init.h"
#include "../a3dapi/native_capacity.h"
#include <type_traits>

namespace cse { namespace a3d_abi {
using abi::LayoutHash;
constexpr std::uint32_t Revision = 1;
constexpr std::uint32_t ProviderRole = 0x41334450; // A3DP, owned native software provider.
constexpr std::uint32_t EventContract = 0x41334445;
constexpr std::uint32_t NotifyContract = 0x4133444e;
constexpr std::uint32_t RecordContract = 0x41334452;

constexpr std::uint32_t FactoryContract = 0x41334443;
constexpr std::uint32_t DeviceContract = 0x41334444;
constexpr std::uint32_t BufferContract = 0x41334442;
constexpr std::uint32_t SpatialBufferContract = 0x41334433;
constexpr std::uint32_t ApiContract = 0x41334441;
constexpr std::uint32_t SourceContract = 0x41334453;
constexpr std::uint32_t GeometryContract = 0x41334447;
constexpr std::uint32_t ListenerContract = 0x4133444c;
constexpr std::uint32_t MaterialListContract = 0x4133444d;
constexpr std::uint32_t PrivateDeviceContract = 0x41334449;
constexpr std::uint32_t ExportContract = 0x41334458;
constexpr std::uint32_t IdentityContract = 0x41334455;

// Stable tags in the V1 normalized call/record shape schema.
enum Shape : std::uint64_t {
    VoidShape = 0, ConstShape = 1, PointerShape = 2, ReferenceShape = 3,
    ArrayShape = 4, RecordShape = 5, ScalarShape = 6, OpaqueShape = 7
};
constexpr std::uint64_t NoDestructorSlots = 0;

template<class T, class = void> struct Complete : std::false_type {};
template<class T> struct Complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

template<class T> constexpr std::uint64_t Type() {
    if constexpr (std::is_void_v<T>) {
        return LayoutHash({VoidShape});
    } else if constexpr (std::is_const_v<T>) {
        return LayoutHash({ConstShape, Type<std::remove_const_t<T>>()});
    } else if constexpr (std::is_pointer_v<T>) {
        return LayoutHash({PointerShape, sizeof(T), alignof(T), Type<std::remove_pointer_t<T>>()});
    } else if constexpr (std::is_reference_v<T>) {
        return LayoutHash({ReferenceShape, sizeof(void*), alignof(void*), Type<std::remove_reference_t<T>>()});
    } else if constexpr (std::is_array_v<T>) {
        return LayoutHash({ArrayShape, std::extent_v<T>, Type<std::remove_extent_t<T>>()});
    } else if constexpr (!Complete<T>::value) {
        return LayoutHash({OpaqueShape});
    } else if constexpr (std::is_class_v<T>) {
        return LayoutHash({RecordShape, sizeof(T), alignof(T), std::is_polymorphic_v<T>,
            std::has_virtual_destructor_v<T>});
    } else {
        return LayoutHash({ScalarShape, sizeof(T), alignof(T), std::is_signed_v<T>,
            std::is_floating_point_v<T>, std::is_enum_v<T>});
    }
}
using GetClassObject = HRESULT (CSE_ABI_CALL *)(REFCLSID, REFIID, void**);
using CanUnloadNow = HRESULT (CSE_ABI_CALL *)();
using CreateNativeEvent = HRESULT (CSE_ABI_CALL *)(const A3dNativeEventDescV1*, IA3dNativeEventV1**);
template<class T> struct Function;
template<class R, class... A> struct Function<R (*)(A...)> {
    static constexpr auto Signature = LayoutHash({Revision, sizeof...(A), Type<R>(), Type<A>()...});
};
constexpr std::uint64_t Guid(const GUID& value) {
    return LayoutHash({value.Data1, value.Data2, value.Data3, value.Data4[0], value.Data4[1],
        value.Data4[2], value.Data4[3], value.Data4[4], value.Data4[5], value.Data4[6], value.Data4[7]});
}
#if defined(__APPLE__)
constexpr std::uint32_t CallProfile = 1; // Darwin native C and Itanium C++ ABI.
#else
constexpr std::uint32_t CallProfile = 2; // ELF native C and Itanium C++ ABI.
#endif

template<class T> struct Method;
template<class C, class R, class... A> struct Method<R (C::*)(A...)> {
    static constexpr auto Signature = LayoutHash({Revision, NoDestructorSlots,
        sizeof...(A), Type<R>(), Type<A>()...});
};
constexpr std::uint64_t Slots(std::initializer_list<std::uint64_t> methods) {
    std::uint64_t result = LayoutHash({Revision, methods.size(), NoDestructorSlots});
    std::uint64_t slot = 0;
    for (auto signature : methods) {
        result = LayoutHash({result, slot++, signature});
    }
    return result;
}
#define CSE_A3D_FIELD(record, field) offsetof(record, field), sizeof(((record*)nullptr)->field), \
    alignof(decltype(record::field)), Type<decltype(record::field)>()
constexpr auto Records = LayoutHash({Revision,
    sizeof(GUID), alignof(GUID),
    CSE_A3D_FIELD(GUID, Data1),
    CSE_A3D_FIELD(GUID, Data2),
    CSE_A3D_FIELD(GUID, Data3),
    CSE_A3D_FIELD(GUID, Data4),
    sizeof(WAVEFORMAT), alignof(WAVEFORMAT),
    CSE_A3D_FIELD(WAVEFORMAT, wFormatTag),
    CSE_A3D_FIELD(WAVEFORMAT, nChannels),
    CSE_A3D_FIELD(WAVEFORMAT, nSamplesPerSec),
    CSE_A3D_FIELD(WAVEFORMAT, nAvgBytesPerSec),
    CSE_A3D_FIELD(WAVEFORMAT, nBlockAlign),
    sizeof(PCMWAVEFORMAT), alignof(PCMWAVEFORMAT),
    CSE_A3D_FIELD(PCMWAVEFORMAT, wf),
    CSE_A3D_FIELD(PCMWAVEFORMAT, wBitsPerSample),
    sizeof(WAVEFORMATEX), alignof(WAVEFORMATEX),
    CSE_A3D_FIELD(WAVEFORMATEX, wFormatTag),
    CSE_A3D_FIELD(WAVEFORMATEX, nChannels),
    CSE_A3D_FIELD(WAVEFORMATEX, nSamplesPerSec),
    CSE_A3D_FIELD(WAVEFORMATEX, nAvgBytesPerSec),
    CSE_A3D_FIELD(WAVEFORMATEX, nBlockAlign),
    CSE_A3D_FIELD(WAVEFORMATEX, wBitsPerSample),
    CSE_A3D_FIELD(WAVEFORMATEX, cbSize),
    sizeof(DSBUFFERDESC1), alignof(DSBUFFERDESC1),
    CSE_A3D_FIELD(DSBUFFERDESC1, dwSize),
    CSE_A3D_FIELD(DSBUFFERDESC1, dwFlags),
    CSE_A3D_FIELD(DSBUFFERDESC1, dwBufferBytes),
    CSE_A3D_FIELD(DSBUFFERDESC1, dwReserved),
    CSE_A3D_FIELD(DSBUFFERDESC1, lpwfxFormat),
    sizeof(DSBUFFERDESC), alignof(DSBUFFERDESC),
    CSE_A3D_FIELD(DSBUFFERDESC, dwSize),
    CSE_A3D_FIELD(DSBUFFERDESC, dwFlags),
    CSE_A3D_FIELD(DSBUFFERDESC, dwBufferBytes),
    CSE_A3D_FIELD(DSBUFFERDESC, dwReserved),
    CSE_A3D_FIELD(DSBUFFERDESC, lpwfxFormat),
    CSE_A3D_FIELD(DSBUFFERDESC, guid3DAlgorithm),
    sizeof(DSBCAPS), alignof(DSBCAPS),
    CSE_A3D_FIELD(DSBCAPS, dwSize),
    CSE_A3D_FIELD(DSBCAPS, dwFlags),
    CSE_A3D_FIELD(DSBCAPS, dwBufferBytes),
    CSE_A3D_FIELD(DSBCAPS, dwUnlockTransferRate),
    CSE_A3D_FIELD(DSBCAPS, dwPlayCpuOverhead),
    sizeof(D3DVECTOR), alignof(D3DVECTOR),
    CSE_A3D_FIELD(D3DVECTOR, x),
    CSE_A3D_FIELD(D3DVECTOR, y),
    CSE_A3D_FIELD(D3DVECTOR, z),
    sizeof(DS3DBUFFER), alignof(DS3DBUFFER),
    CSE_A3D_FIELD(DS3DBUFFER, dwSize),
    CSE_A3D_FIELD(DS3DBUFFER, vPosition),
    CSE_A3D_FIELD(DS3DBUFFER, vVelocity),
    CSE_A3D_FIELD(DS3DBUFFER, dwInsideConeAngle),
    CSE_A3D_FIELD(DS3DBUFFER, dwOutsideConeAngle),
    CSE_A3D_FIELD(DS3DBUFFER, vConeOrientation),
    CSE_A3D_FIELD(DS3DBUFFER, lConeOutsideVolume),
    CSE_A3D_FIELD(DS3DBUFFER, flMinDistance),
    CSE_A3D_FIELD(DS3DBUFFER, flMaxDistance),
    CSE_A3D_FIELD(DS3DBUFFER, dwMode),
    sizeof(A3dNativeEventDescV1), alignof(A3dNativeEventDescV1),
    CSE_A3D_FIELD(A3dNativeEventDescV1, structureBytes),
    CSE_A3D_FIELD(A3dNativeEventDescV1, version),
    CSE_A3D_FIELD(A3dNativeEventDescV1, resetMode),
    CSE_A3D_FIELD(A3dNativeEventDescV1, initiallySignaled),
    sizeof(A3dNativeNotificationPositionV1), alignof(A3dNativeNotificationPositionV1),
    CSE_A3D_FIELD(A3dNativeNotificationPositionV1, offsetBytes),
    CSE_A3D_FIELD(A3dNativeNotificationPositionV1, reserved),
    CSE_A3D_FIELD(A3dNativeNotificationPositionV1, event),
    sizeof(A3dNativeNotificationCapsV1), alignof(A3dNativeNotificationCapsV1),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, structureBytes),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, version),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, flags),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, maximumPositions),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, cursorKind),
    CSE_A3D_FIELD(A3dNativeNotificationCapsV1, reserved),
    sizeof(DSCAPS), alignof(DSCAPS),
    CSE_A3D_FIELD(DSCAPS, dwSize),
    CSE_A3D_FIELD(DSCAPS, dwFlags),
    CSE_A3D_FIELD(DSCAPS, dwMinSecondarySampleRate),
    CSE_A3D_FIELD(DSCAPS, dwMaxSecondarySampleRate),
    CSE_A3D_FIELD(DSCAPS, dwPrimaryBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHwMixingAllBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHwMixingStaticBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHwMixingStreamingBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHwMixingAllBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHwMixingStaticBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHwMixingStreamingBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHw3DAllBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHw3DStaticBuffers),
    CSE_A3D_FIELD(DSCAPS, dwMaxHw3DStreamingBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHw3DAllBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHw3DStaticBuffers),
    CSE_A3D_FIELD(DSCAPS, dwFreeHw3DStreamingBuffers),
    CSE_A3D_FIELD(DSCAPS, dwTotalHwMemBytes),
    CSE_A3D_FIELD(DSCAPS, dwFreeHwMemBytes),
    CSE_A3D_FIELD(DSCAPS, dwMaxContigFreeHwMemBytes),
    CSE_A3D_FIELD(DSCAPS, dwUnlockTransferRateHwBuffers),
    CSE_A3D_FIELD(DSCAPS, dwPlayCpuOverheadSwBuffers),
    CSE_A3D_FIELD(DSCAPS, dwReserved1),
    CSE_A3D_FIELD(DSCAPS, dwReserved2),
    sizeof(A3DCAPS_SOFTWARE), alignof(A3DCAPS_SOFTWARE),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwSize),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwVersion),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwFlags),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwReserved),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwReserved2),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwOutputChannels),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwMinSampleRate),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwMaxSampleRate),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwMax2DBuffers),
    CSE_A3D_FIELD(A3DCAPS_SOFTWARE, dwMax3DBuffers),
    sizeof(A3DCAPS_HARDWARE), alignof(A3DCAPS_HARDWARE),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwSize),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwFlags),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwReserved),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwReserved2),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwOutputChannels),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwMinSampleRate),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwMaxSampleRate),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwMax2DBuffers),
    CSE_A3D_FIELD(A3DCAPS_HARDWARE, dwMax3DBuffers),
    Type<HRESULT>(), Type<ULONG>(), Type<DWORD>(), Type<BOOL>(), Type<A3DVAL>()});
#undef CSE_A3D_FIELD

template<class Source, class Geometry, class Listener> constexpr abi::Descriptor Describe() {
    static_assert(!std::has_virtual_destructor_v<IUnknown>, "COM views have no destructor slots");
    constexpr auto IUnknownSlots = Slots({
        Method<decltype(&IUnknown::QueryInterface)>::Signature,
        Method<decltype(&IUnknown::AddRef)>::Signature,
        Method<decltype(&IUnknown::Release)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IClassFactory>, "COM views have no destructor slots");
    constexpr auto IClassFactorySlots = Slots({
        Method<decltype(&IClassFactory::QueryInterface)>::Signature,
        Method<decltype(&IClassFactory::AddRef)>::Signature,
        Method<decltype(&IClassFactory::Release)>::Signature,
        Method<decltype(&IClassFactory::CreateInstance)>::Signature,
        Method<decltype(&IClassFactory::LockServer)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IDirectSound>, "COM views have no destructor slots");
    constexpr auto IDirectSoundSlots = Slots({
        Method<decltype(&IDirectSound::QueryInterface)>::Signature,
        Method<decltype(&IDirectSound::AddRef)>::Signature,
        Method<decltype(&IDirectSound::Release)>::Signature,
        Method<decltype(&IDirectSound::CreateSoundBuffer)>::Signature,
        Method<decltype(&IDirectSound::GetCaps)>::Signature,
        Method<decltype(&IDirectSound::DuplicateSoundBuffer)>::Signature,
        Method<decltype(&IDirectSound::SetCooperativeLevel)>::Signature,
        Method<decltype(&IDirectSound::Compact)>::Signature,
        Method<decltype(&IDirectSound::GetSpeakerConfig)>::Signature,
        Method<decltype(&IDirectSound::SetSpeakerConfig)>::Signature,
        Method<decltype(&IDirectSound::Initialize)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IDirectSoundBuffer>, "COM views have no destructor slots");
    constexpr auto IDirectSoundBufferSlots = Slots({
        Method<decltype(&IDirectSoundBuffer::QueryInterface)>::Signature,
        Method<decltype(&IDirectSoundBuffer::AddRef)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Release)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetCaps)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetCurrentPosition)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetFormat)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetVolume)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetPan)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetFrequency)>::Signature,
        Method<decltype(&IDirectSoundBuffer::GetStatus)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Initialize)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Lock)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Play)>::Signature,
        Method<decltype(&IDirectSoundBuffer::SetCurrentPosition)>::Signature,
        Method<decltype(&IDirectSoundBuffer::SetFormat)>::Signature,
        Method<decltype(&IDirectSoundBuffer::SetVolume)>::Signature,
        Method<decltype(&IDirectSoundBuffer::SetPan)>::Signature,
        Method<decltype(&IDirectSoundBuffer::SetFrequency)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Stop)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Unlock)>::Signature,
        Method<decltype(&IDirectSoundBuffer::Restore)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IDirectSound3DBuffer>, "COM views have no destructor slots");
    constexpr auto IDirectSound3DBufferSlots = Slots({
        Method<decltype(&IDirectSound3DBuffer::QueryInterface)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::AddRef)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::Release)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetAllParameters)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetConeAngles)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetConeOrientation)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetConeOutsideVolume)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetMaxDistance)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetMinDistance)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetMode)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetPosition)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::GetVelocity)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetAllParameters)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetConeAngles)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetConeOrientation)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetConeOutsideVolume)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetMaxDistance)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetMinDistance)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetMode)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetPosition)>::Signature,
        Method<decltype(&IDirectSound3DBuffer::SetVelocity)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3d3>, "COM views have no destructor slots");
    constexpr auto IA3d3Slots = Slots({
        Method<decltype(&IA3d3::QueryInterface)>::Signature,
        Method<decltype(&IA3d3::AddRef)>::Signature,
        Method<decltype(&IA3d3::Release)>::Signature,
        Method<decltype(&IA3d3::SetOutputMode)>::Signature,
        Method<decltype(&IA3d3::GetOutputMode)>::Signature,
        Method<decltype(&IA3d3::SetResourceManagerMode)>::Signature,
        Method<decltype(&IA3d3::GetResourceManagerMode)>::Signature,
        Method<decltype(&IA3d3::SetHFAbsorbFactor)>::Signature,
        Method<decltype(&IA3d3::GetHFAbsorbFactor)>::Signature,
        Method<decltype(&IA3d3::RegisterVersion)>::Signature,
        Method<decltype(&IA3d3::GetSoftwareCaps)>::Signature,
        Method<decltype(&IA3d3::GetHardwareCaps)>::Signature,
        Method<decltype(&IA3d3::Clear)>::Signature,
        Method<decltype(&IA3d3::Flush)>::Signature,
        Method<decltype(&IA3d3::Compat)>::Signature,
        Method<decltype(&IA3d3::Init)>::Signature,
        Method<decltype(&IA3d3::IsFeatureAvailable)>::Signature,
        Method<decltype(&IA3d3::NewSource)>::Signature,
        Method<decltype(&IA3d3::DuplicateSource)>::Signature,
        Method<decltype(&IA3d3::SetCooperativeLevel)>::Signature,
        Method<decltype(&IA3d3::GetCooperativeLevel)>::Signature,
        Method<decltype(&IA3d3::SetMaxReflectionDelayTime)>::Signature,
        Method<decltype(&IA3d3::GetMaxReflectionDelayTime)>::Signature,
        Method<decltype(&IA3d3::SetCoordinateSystem)>::Signature,
        Method<decltype(&IA3d3::GetCoordinateSystem)>::Signature,
        Method<decltype(&IA3d3::SetOutputGain)>::Signature,
        Method<decltype(&IA3d3::GetOutputGain)>::Signature,
        Method<decltype(&IA3d3::SetNumFallbackSources)>::Signature,
        Method<decltype(&IA3d3::GetNumFallbackSources)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3d2>, "COM views have no destructor slots");
    constexpr auto IA3d2Slots = Slots({
        Method<decltype(&IA3d2::QueryInterface)>::Signature,
        Method<decltype(&IA3d2::AddRef)>::Signature,
        Method<decltype(&IA3d2::Release)>::Signature,
        Method<decltype(&IA3d2::SetOutputMode)>::Signature,
        Method<decltype(&IA3d2::GetOutputMode)>::Signature,
        Method<decltype(&IA3d2::SetResourceManagerMode)>::Signature,
        Method<decltype(&IA3d2::GetResourceManagerMode)>::Signature,
        Method<decltype(&IA3d2::SetHFAbsorbFactor)>::Signature,
        Method<decltype(&IA3d2::GetHFAbsorbFactor)>::Signature,
        Method<decltype(&IA3d2::RegisterVersion)>::Signature,
        Method<decltype(&IA3d2::GetSoftwareCaps)>::Signature,
        Method<decltype(&IA3d2::GetHardwareCaps)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<Geometry>, "COM views have no destructor slots");
    constexpr auto IA3dGeomTargetSlots = Slots({
        Method<decltype(&Geometry::QueryInterface)>::Signature,
        Method<decltype(&Geometry::AddRef)>::Signature,
        Method<decltype(&Geometry::Release)>::Signature,
        Method<decltype(&Geometry::Enable)>::Signature,
        Method<decltype(&Geometry::Disable)>::Signature,
        Method<decltype(&Geometry::IsEnabled)>::Signature,
        Method<decltype(&Geometry::SetOcclusionMode)>::Signature,
        Method<decltype(&Geometry::GetOcclusionMode)>::Signature,
        Method<decltype(&Geometry::SetReflectionMode)>::Signature,
        Method<decltype(&Geometry::GetReflectionMode)>::Signature,
        Method<decltype(&Geometry::SetReflectionGainScale)>::Signature,
        Method<decltype(&Geometry::GetReflectionGainScale)>::Signature,
        Method<decltype(&Geometry::SetReflectionDelayScale)>::Signature,
        Method<decltype(&Geometry::GetReflectionDelayScale)>::Signature,
        Method<decltype(&Geometry::PushMatrix)>::Signature,
        Method<decltype(&Geometry::PopMatrix)>::Signature,
        Method<decltype(&Geometry::LoadIdentity)>::Signature,
        Method<decltype(&Geometry::LoadMatrix)>::Signature,
        Method<decltype(&Geometry::GetMatrix)>::Signature,
        Method<decltype(&Geometry::MultMatrix)>::Signature,
        Method<decltype(&Geometry::Translate3f)>::Signature,
        Method<decltype(&Geometry::Translate3fv)>::Signature,
        Method<decltype(&Geometry::Rotate3f)>::Signature,
        Method<decltype(&Geometry::Rotate3fv)>::Signature,
        Method<decltype(&Geometry::Scale3f)>::Signature,
        Method<decltype(&Geometry::Scale3fv)>::Signature,
        Method<decltype(&Geometry::Begin)>::Signature,
        Method<decltype(&Geometry::End)>::Signature,
        Method<decltype(&Geometry::Vertex3f)>::Signature,
        Method<decltype(&Geometry::Vertex3fv)>::Signature,
        Method<decltype(&Geometry::Normal3f)>::Signature,
        Method<decltype(&Geometry::Normal3fv)>::Signature,
        Method<decltype(&Geometry::Tag)>::Signature,
        Method<decltype(&Geometry::SetOpeningFactorf)>::Signature,
        Method<decltype(&Geometry::SetOpeningFactorfv)>::Signature,
        Method<decltype(&Geometry::NewMaterial)>::Signature,
        Method<decltype(&Geometry::BindMaterial)>::Signature,
        Method<decltype(&Geometry::NewList)>::Signature,
        Method<decltype(&Geometry::BindListener)>::Signature,
        Method<decltype(&Geometry::BindSource)>::Signature,
        Method<decltype(&Geometry::NewEnvironment)>::Signature,
        Method<decltype(&Geometry::BindEnvironment)>::Signature,
        Method<decltype(&Geometry::SetRenderMode)>::Signature,
        Method<decltype(&Geometry::GetRenderMode)>::Signature,
        Method<decltype(&Geometry::SetPolygonBloatFactor)>::Signature,
        Method<decltype(&Geometry::GetPolygonBloatFactor)>::Signature,
        Method<decltype(&Geometry::SetReflectionUpdateInterval)>::Signature,
        Method<decltype(&Geometry::GetReflectionUpdateInterval)>::Signature,
        Method<decltype(&Geometry::SetOcclusionUpdateInterval)>::Signature,
        Method<decltype(&Geometry::GetOcclusionUpdateInterval)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<Listener>, "COM views have no destructor slots");
    constexpr auto IA3dListenerTargetSlots = Slots({
        Method<decltype(&Listener::QueryInterface)>::Signature,
        Method<decltype(&Listener::AddRef)>::Signature,
        Method<decltype(&Listener::Release)>::Signature,
        Method<decltype(&Listener::ListenerSetPosition3f)>::Signature,
        Method<decltype(&Listener::ListenerGetPosition3f)>::Signature,
        Method<decltype(&Listener::ListenerSetPosition3fv)>::Signature,
        Method<decltype(&Listener::ListenerGetPosition3fv)>::Signature,
        Method<decltype(&Listener::ListenerSetOrientationAngles3f)>::Signature,
        Method<decltype(&Listener::ListenerGetOrientationAngles3f)>::Signature,
        Method<decltype(&Listener::ListenerSetOrientationAngles3fv)>::Signature,
        Method<decltype(&Listener::ListenerGetOrientationAngles3fv)>::Signature,
        Method<decltype(&Listener::ListenerSetOrientation6f)>::Signature,
        Method<decltype(&Listener::ListenerGetOrientation6f)>::Signature,
        Method<decltype(&Listener::ListenerSetOrientation6fv)>::Signature,
        Method<decltype(&Listener::ListenerGetOrientation6fv)>::Signature,
        Method<decltype(&Listener::ListenerSetVelocity3f)>::Signature,
        Method<decltype(&Listener::ListenerGetVelocity3f)>::Signature,
        Method<decltype(&Listener::ListenerSetVelocity3fv)>::Signature,
        Method<decltype(&Listener::ListenerGetVelocity3fv)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<Source>, "COM views have no destructor slots");
    constexpr auto IA3dSource202Slots = Slots({
        Method<decltype(&Source::QueryInterface)>::Signature,
        Method<decltype(&Source::AddRef)>::Signature,
        Method<decltype(&Source::Release)>::Signature,
        Method<decltype(&Source::LoadWaveFile)>::Signature,
        Method<decltype(&Source::LoadWaveData)>::Signature,
        Method<decltype(&Source::AllocateWaveData)>::Signature,
        Method<decltype(&Source::FreeWaveData)>::Signature,
        Method<decltype(&Source::SetWaveFormat)>::Signature,
        Method<decltype(&Source::GetWaveFormat)>::Signature,
        Method<decltype(&Source::GetWaveSize)>::Signature,
        Method<decltype(&Source::GetType)>::Signature,
        Method<decltype(&Source::Lock)>::Signature,
        Method<decltype(&Source::Unlock)>::Signature,
        Method<decltype(&Source::Play)>::Signature,
        Method<decltype(&Source::Stop)>::Signature,
        Method<decltype(&Source::Rewind)>::Signature,
        Method<decltype(&Source::SetWaveTime)>::Signature,
        Method<decltype(&Source::GetWaveTime)>::Signature,
        Method<decltype(&Source::SetWavePosition)>::Signature,
        Method<decltype(&Source::GetWavePosition)>::Signature,
        Method<decltype(&Source::SetPosition3f)>::Signature,
        Method<decltype(&Source::GetPosition3f)>::Signature,
        Method<decltype(&Source::SetPosition3fv)>::Signature,
        Method<decltype(&Source::GetPosition3fv)>::Signature,
        Method<decltype(&Source::SetOrientationAngles3f)>::Signature,
        Method<decltype(&Source::GetOrientationAngles3f)>::Signature,
        Method<decltype(&Source::SetOrientationAngles3fv)>::Signature,
        Method<decltype(&Source::GetOrientationAngles3fv)>::Signature,
        Method<decltype(&Source::SetOrientation6f)>::Signature,
        Method<decltype(&Source::GetOrientation6f)>::Signature,
        Method<decltype(&Source::SetOrientation6fv)>::Signature,
        Method<decltype(&Source::GetOrientation6fv)>::Signature,
        Method<decltype(&Source::SetVelocity3f)>::Signature,
        Method<decltype(&Source::GetVelocity3f)>::Signature,
        Method<decltype(&Source::SetVelocity3fv)>::Signature,
        Method<decltype(&Source::GetVelocity3fv)>::Signature,
        Method<decltype(&Source::SetCone)>::Signature,
        Method<decltype(&Source::GetCone)>::Signature,
        Method<decltype(&Source::SetMinMaxDistance)>::Signature,
        Method<decltype(&Source::GetMinMaxDistance)>::Signature,
        Method<decltype(&Source::SetGain)>::Signature,
        Method<decltype(&Source::GetGain)>::Signature,
        Method<decltype(&Source::SetPitch)>::Signature,
        Method<decltype(&Source::GetPitch)>::Signature,
        Method<decltype(&Source::SetDopplerScale)>::Signature,
        Method<decltype(&Source::GetDopplerScale)>::Signature,
        Method<decltype(&Source::SetDistanceModelScale)>::Signature,
        Method<decltype(&Source::GetDistanceModelScale)>::Signature,
        Method<decltype(&Source::SetEq)>::Signature,
        Method<decltype(&Source::GetEq)>::Signature,
        Method<decltype(&Source::SetPriority)>::Signature,
        Method<decltype(&Source::GetPriority)>::Signature,
        Method<decltype(&Source::SetRenderMode)>::Signature,
        Method<decltype(&Source::GetRenderMode)>::Signature,
        Method<decltype(&Source::GetAudibility)>::Signature,
        Method<decltype(&Source::GetOcclusionFactor)>::Signature,
        Method<decltype(&Source::GetStatus)>::Signature,
        Method<decltype(&Source::SetPanValues)>::Signature,
        Method<decltype(&Source::GetPanValues)>::Signature,
        Method<decltype(&Source::SetWaveEvent)>::Signature,
        Method<decltype(&Source::ClearWaveEvents)>::Signature,
        Method<decltype(&Source::SetTransformMode)>::Signature,
        Method<decltype(&Source::GetTransformMode)>::Signature,
        Method<decltype(&Source::SetReflectionDelayScale)>::Signature,
        Method<decltype(&Source::GetReflectionDelayScale)>::Signature,
        Method<decltype(&Source::SetReflectionGainScale)>::Signature,
        Method<decltype(&Source::GetReflectionGainScale)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dList>, "COM views have no destructor slots");
    constexpr auto IA3dListSlots = Slots({
        Method<decltype(&IA3dList::QueryInterface)>::Signature,
        Method<decltype(&IA3dList::AddRef)>::Signature,
        Method<decltype(&IA3dList::Release)>::Signature,
        Method<decltype(&IA3dList::Begin)>::Signature,
        Method<decltype(&IA3dList::End)>::Signature,
        Method<decltype(&IA3dList::Call)>::Signature,
        Method<decltype(&IA3dList::EnableBoundingVol)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dMaterial>, "COM views have no destructor slots");
    constexpr auto IA3dMaterialSlots = Slots({
        Method<decltype(&IA3dMaterial::QueryInterface)>::Signature,
        Method<decltype(&IA3dMaterial::AddRef)>::Signature,
        Method<decltype(&IA3dMaterial::Release)>::Signature,
        Method<decltype(&IA3dMaterial::Load)>::Signature,
        Method<decltype(&IA3dMaterial::Save)>::Signature,
        Method<decltype(&IA3dMaterial::UnSerialize)>::Signature,
        Method<decltype(&IA3dMaterial::Serialize)>::Signature,
        Method<decltype(&IA3dMaterial::Duplicate)>::Signature,
        Method<decltype(&IA3dMaterial::SetNameID)>::Signature,
        Method<decltype(&IA3dMaterial::GetNameID)>::Signature,
        Method<decltype(&IA3dMaterial::SelectPreset)>::Signature,
        Method<decltype(&IA3dMaterial::GetClosestPreset)>::Signature,
        Method<decltype(&IA3dMaterial::SetReflectance)>::Signature,
        Method<decltype(&IA3dMaterial::GetReflectance)>::Signature,
        Method<decltype(&IA3dMaterial::SetTransmittance)>::Signature,
        Method<decltype(&IA3dMaterial::GetTransmittance)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dNativeDeviceV1>, "COM views have no destructor slots");
    constexpr auto IA3dNativeDeviceV1Slots = Slots({
        Method<decltype(&IA3dNativeDeviceV1::QueryInterface)>::Signature,
        Method<decltype(&IA3dNativeDeviceV1::AddRef)>::Signature,
        Method<decltype(&IA3dNativeDeviceV1::Release)>::Signature,
        Method<decltype(&IA3dNativeDeviceV1::InitializeDevice)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dNativeCapacityV1>, "COM views have no destructor slots");
    constexpr auto IA3dNativeCapacityV1Slots = Slots({
        Method<decltype(&IA3dNativeCapacityV1::QueryInterface)>::Signature,
        Method<decltype(&IA3dNativeCapacityV1::AddRef)>::Signature,
        Method<decltype(&IA3dNativeCapacityV1::Release)>::Signature,
        Method<decltype(&IA3dNativeCapacityV1::GetVoiceCapacity)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dNativeEventV1>, "COM views have no destructor slots");
    constexpr auto IA3dNativeEventV1Slots = Slots({
        Method<decltype(&IA3dNativeEventV1::QueryInterface)>::Signature,
        Method<decltype(&IA3dNativeEventV1::AddRef)>::Signature,
        Method<decltype(&IA3dNativeEventV1::Release)>::Signature,
        Method<decltype(&IA3dNativeEventV1::Signal)>::Signature,
        Method<decltype(&IA3dNativeEventV1::Reset)>::Signature,
        Method<decltype(&IA3dNativeEventV1::Close)>::Signature,
        Method<decltype(&IA3dNativeEventV1::Wait)>::Signature,
    });
    static_assert(!std::has_virtual_destructor_v<IA3dNativeNotifyV1>, "COM views have no destructor slots");
    constexpr auto IA3dNativeNotifyV1Slots = Slots({
        Method<decltype(&IA3dNativeNotifyV1::QueryInterface)>::Signature,
        Method<decltype(&IA3dNativeNotifyV1::AddRef)>::Signature,
        Method<decltype(&IA3dNativeNotifyV1::Release)>::Signature,
        Method<decltype(&IA3dNativeNotifyV1::GetNotificationCaps)>::Signature,
        Method<decltype(&IA3dNativeNotifyV1::SetNotificationPositions)>::Signature,
    });
    return abi::Describe({
        {ProviderRole, 0, LayoutHash({Revision, CallProfile, 1 /* owned software */, 1 /* host library lease */})},
        {RecordContract, 0, Records},
        {FactoryContract, 0, LayoutHash({IUnknownSlots, IClassFactorySlots})},
        {DeviceContract, 0, IDirectSoundSlots},
        {BufferContract, 0, IDirectSoundBufferSlots},
        {SpatialBufferContract, 0, IDirectSound3DBufferSlots},
        {ApiContract, 0, LayoutHash({IA3d3Slots, IA3d2Slots})},
        {SourceContract, 0, IA3dSource202Slots},
        {GeometryContract, 0, IA3dGeomTargetSlots},
        {ListenerContract, 0, IA3dListenerTargetSlots},
        {MaterialListContract, 0, LayoutHash({IA3dMaterialSlots, IA3dListSlots})},
        {EventContract, 0, LayoutHash({A3D_NATIVE_NOTIFICATION_VERSION, IA3dNativeEventV1Slots,
            Guid(IID_IA3dNativeEventV1),
            A3D_NATIVE_EVENT_MANUAL_RESET, A3D_NATIVE_WAIT_INFINITE,
            static_cast<std::uint32_t>(A3dNativeWaitStatusV1::Closed)})},
        {NotifyContract, 0, LayoutHash({A3D_NATIVE_NOTIFICATION_VERSION, IA3dNativeNotifyV1Slots,
            Guid(IID_IA3dNativeNotifyV1),
            A3D_NATIVE_NOTIFICATION_MAX_POSITIONS, A3D_NATIVE_NOTIFICATION_STOP,
            A3D_NATIVE_CURSOR_RENDER_CONSUMED, A3D_NATIVE_CURSOR_STREAM_SUBMITTED,
            A3D_NATIVE_NOTIFY_BYTE_OFFSETS, A3D_NATIVE_NOTIFY_STOP, A3D_NATIVE_NOTIFY_WRAP,
            A3D_NATIVE_NOTIFY_ATOMIC_REPLACE, A3D_NATIVE_NOTIFY_RETAINED_EVENTS})},
        {PrivateDeviceContract, 0, LayoutHash({IA3dNativeDeviceV1Slots, IA3dNativeCapacityV1Slots,
            Guid(IID_IA3dNativeDeviceV1), Guid(IID_IA3dNativeCapacityV1)})},
        {ExportContract, 0, LayoutHash({Function<GetClassObject>::Signature, Function<CanUnloadNow>::Signature,
            Function<CreateNativeEvent>::Signature, Function<abi::GetModuleAbi>::Signature})},
        {IdentityContract, 0, LayoutHash({Guid(CLSID_A3dApi), Guid(CLSID_A3d), Guid(IID_IUnknown),
            Guid(IID_IClassFactory), Guid(IID_IA3d), Guid(IID_IA3d2), Guid(IID_IA3d3),
            Guid(IID_IA3dSource), Guid(IID_IA3dGeom), Guid(IID_IA3dListener), Guid(IID_IA3dMaterial),
            Guid(IID_IA3dList), Guid(IID_IDirectSound), Guid(IID_IDirectSoundBuffer),
            Guid(IID_IDirectSound3DBuffer), Guid(IID_IDirectSoundNotify)})}
    });
}
} } // namespace cse::a3d_abi
