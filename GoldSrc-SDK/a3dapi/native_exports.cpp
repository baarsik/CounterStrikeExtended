#include "A3d3.h"
#include "A3dSource.h"
#include "../public/cse_a3d_abi.h"

namespace {
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::QueryInterface)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::QueryInterface)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::AddRef)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::AddRef)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Release)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Release)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::LoadWaveFile)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::LoadWaveFile)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::LoadWaveData)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::LoadWaveData)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::AllocateWaveData)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::AllocateWaveData)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::FreeWaveData)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::FreeWaveData)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetWaveFormat)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetWaveFormat)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetWaveFormat)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetWaveFormat)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetWaveSize)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetWaveSize)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetType)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetType)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Lock)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Lock)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Unlock)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Unlock)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Play)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Play)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Stop)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Stop)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::Rewind)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::Rewind)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetWaveTime)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetWaveTime)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetWaveTime)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetWaveTime)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetWavePosition)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetWavePosition)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetWavePosition)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetWavePosition)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetPosition3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetPosition3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetPosition3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetPosition3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetPosition3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetPosition3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetPosition3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetPosition3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetOrientationAngles3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetOrientationAngles3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetOrientationAngles3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetOrientationAngles3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetOrientationAngles3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetOrientationAngles3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetOrientationAngles3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetOrientationAngles3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetOrientation6f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetOrientation6f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetOrientation6f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetOrientation6f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetOrientation6fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetOrientation6fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetOrientation6fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetOrientation6fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetVelocity3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetVelocity3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetVelocity3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetVelocity3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetVelocity3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetVelocity3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetVelocity3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetVelocity3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetCone)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetCone)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetCone)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetCone)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetMinMaxDistance)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetMinMaxDistance)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetMinMaxDistance)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetMinMaxDistance)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetGain)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetGain)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetGain)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetGain)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetPitch)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetPitch)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetPitch)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetPitch)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetDopplerScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetDopplerScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetDopplerScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetDopplerScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetDistanceModelScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetDistanceModelScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetDistanceModelScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetDistanceModelScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetEq)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetEq)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetEq)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetEq)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetPriority)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetPriority)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetPriority)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetPriority)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetRenderMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetRenderMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetRenderMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetRenderMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetAudibility)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetAudibility)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetOcclusionFactor)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetOcclusionFactor)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetStatus)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetStatus)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetPanValues)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetPanValues)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetPanValues)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetPanValues)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetWaveEvent)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetWaveEvent)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::ClearWaveEvents)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::ClearWaveEvents)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetTransformMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetTransformMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetTransformMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetTransformMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetReflectionDelayScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetReflectionDelayScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetReflectionDelayScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetReflectionDelayScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::SetReflectionGainScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::SetReflectionGainScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dSource::GetReflectionGainScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dSource202::GetReflectionGainScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::QueryInterface)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::QueryInterface)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::AddRef)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::AddRef)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Release)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Release)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Enable)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Enable)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Disable)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Disable)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::IsEnabled)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::IsEnabled)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetOcclusionMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetOcclusionMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetOcclusionMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetOcclusionMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetReflectionMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetReflectionMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetReflectionMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetReflectionMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetReflectionGainScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetReflectionGainScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetReflectionGainScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetReflectionGainScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetReflectionDelayScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetReflectionDelayScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetReflectionDelayScale)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetReflectionDelayScale)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::PushMatrix)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::PushMatrix)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::PopMatrix)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::PopMatrix)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::LoadIdentity)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::LoadIdentity)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::LoadMatrix)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::LoadMatrix)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetMatrix)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetMatrix)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::MultMatrix)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::MultMatrix)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Translate3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Translate3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Translate3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Translate3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Rotate3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Rotate3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Rotate3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Rotate3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Scale3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Scale3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Scale3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Scale3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Begin)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Begin)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::End)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::End)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Vertex3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Vertex3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Vertex3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Vertex3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Normal3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Normal3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Normal3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Normal3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::Tag)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::Tag)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetOpeningFactorf)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetOpeningFactorf)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetOpeningFactorfv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetOpeningFactorfv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::NewMaterial)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::NewMaterial)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::BindMaterial)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::BindMaterial)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::NewList)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::NewList)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::BindListener)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::BindListener)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::BindSource)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::BindSource)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::NewEnvironment)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::NewEnvironment)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::BindEnvironment)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::BindEnvironment)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetRenderMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetRenderMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetRenderMode)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetRenderMode)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetPolygonBloatFactor)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetPolygonBloatFactor)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetPolygonBloatFactor)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetPolygonBloatFactor)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetReflectionUpdateInterval)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetReflectionUpdateInterval)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetReflectionUpdateInterval)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetReflectionUpdateInterval)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::SetOcclusionUpdateInterval)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::SetOcclusionUpdateInterval)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dGeom::GetOcclusionUpdateInterval)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dGeomTarget::GetOcclusionUpdateInterval)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::QueryInterface)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::QueryInterface)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::AddRef)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::AddRef)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::Release)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::Release)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetPosition3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetPosition3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetPosition3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetPosition3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetPosition3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetPosition3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetPosition3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetPosition3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetOrientationAngles3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetOrientationAngles3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetOrientationAngles3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetOrientationAngles3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetOrientationAngles3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetOrientationAngles3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetOrientationAngles3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetOrientationAngles3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetOrientation6f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetOrientation6f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetOrientation6f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetOrientation6f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetOrientation6fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetOrientation6fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetOrientation6fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetOrientation6fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetVelocity3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetVelocity3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetVelocity3f)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetVelocity3f)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::SetVelocity3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerSetVelocity3fv)>::Signature);
static_assert(cse::a3d_abi::Method<decltype(&IA3dListener::GetVelocity3fv)>::Signature ==
    cse::a3d_abi::Method<decltype(&IA3dListenerTarget::ListenerGetVelocity3fv)>::Signature);

constexpr auto descriptor = cse::a3d_abi::Describe<IA3dSource202, IA3dGeomTarget, IA3dListenerTarget>();
}

extern "C" const cse::abi::Descriptor* CSE_ABI_CALL CseGetModuleAbi() {
    return &descriptor;
}

static_assert(std::is_same_v<decltype(&CseGetModuleAbi), cse::abi::GetModuleAbi>);
