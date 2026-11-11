#pragma once
#include "ia3dapi.h"
#include "platform_dsound.h"
class IA3dGeomTarget : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Enable( DWORD dwFeatures ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Disable( DWORD dwFeatures ) = 0;
    virtual BOOL STDMETHODCALLTYPE IsEnabled( DWORD dwFeatures ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOcclusionMode( DWORD dwMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOcclusionMode( LPDWORD pMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionMode( DWORD dwMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionMode( LPDWORD pMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionGainScale( A3DVAL flScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionGainScale( LPA3DVAL pScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionDelayScale( A3DVAL flScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionDelayScale( LPA3DVAL pScale ) = 0;
    virtual ULONG STDMETHODCALLTYPE PushMatrix() = 0;
    virtual ULONG STDMETHODCALLTYPE PopMatrix() = 0;
    virtual HRESULT STDMETHODCALLTYPE LoadIdentity() = 0;
    virtual HRESULT STDMETHODCALLTYPE LoadMatrix( A3DMATRIX matrix ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMatrix( A3DMATRIX matrix ) = 0;
    virtual HRESULT STDMETHODCALLTYPE MultMatrix( A3DMATRIX matrix ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Translate3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Translate3fv( LPA3DVAL pTranslation ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Rotate3f( A3DVAL flAngle, A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Rotate3fv( A3DVAL flAngle, LPA3DVAL pAxis ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Scale3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Scale3fv( LPA3DVAL pScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Begin( DWORD dwMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE End() = 0;
    virtual HRESULT STDMETHODCALLTYPE Vertex3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Vertex3fv( LPA3DVAL pVertex ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Normal3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Normal3fv( LPA3DVAL pNormal ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Tag( DWORD dwTag ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOpeningFactorf( A3DVAL flFactor ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOpeningFactorfv( LPA3DVAL pFactor ) = 0;
    virtual HRESULT STDMETHODCALLTYPE NewMaterial( LPA3DMATERIAL* ppMaterial ) = 0;
    virtual HRESULT STDMETHODCALLTYPE BindMaterial( LPA3DMATERIAL pMaterial ) = 0;
    virtual HRESULT STDMETHODCALLTYPE NewList( LPA3DLIST* ppList ) = 0;
    virtual HRESULT STDMETHODCALLTYPE BindListener() = 0;
    virtual HRESULT STDMETHODCALLTYPE BindSource( LPA3DSOURCE pSource ) = 0;
    virtual HRESULT STDMETHODCALLTYPE NewEnvironment( LPA3DENVIRONMENT* ppEnvironment ) = 0;
    virtual HRESULT STDMETHODCALLTYPE BindEnvironment( LPA3DENVIRONMENT pEnvironment ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRenderMode(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRenderMode(LPDWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPolygonBloatFactor(A3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPolygonBloatFactor(LPA3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionUpdateInterval(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionUpdateInterval(LPDWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOcclusionUpdateInterval(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOcclusionUpdateInterval(LPDWORD) = 0;

};

class IA3dListenerTarget : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE ListenerSetPosition3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetPosition3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetPosition3fv( LPA3DVAL pPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetPosition3fv( LPA3DVAL pPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetOrientationAngles3f( A3DVAL yaw, A3DVAL pitch, A3DVAL roll ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetOrientationAngles3f( LPA3DVAL pYaw, LPA3DVAL pPitch, LPA3DVAL pRoll ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetOrientationAngles3fv( LPA3DVAL pAngles ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetOrientationAngles3fv( LPA3DVAL pAngles ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetOrientation6f( A3DVAL frontX, A3DVAL frontY, A3DVAL frontZ,
        A3DVAL topX, A3DVAL topY, A3DVAL topZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetOrientation6f( LPA3DVAL pFrontX, LPA3DVAL pFrontY, LPA3DVAL pFrontZ,
        LPA3DVAL pTopX, LPA3DVAL pTopY, LPA3DVAL pTopZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetOrientation6fv( LPA3DVAL pOrientation ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetOrientation6fv( LPA3DVAL pOrientation ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetVelocity3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetVelocity3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerSetVelocity3fv( LPA3DVAL pVelocity ) = 0;
    virtual HRESULT STDMETHODCALLTYPE ListenerGetVelocity3fv( LPA3DVAL pVelocity ) = 0;
};

class IA3dSource202 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE LoadWaveFile( LPSTR pFilename ) = 0;
    virtual HRESULT STDMETHODCALLTYPE LoadWaveData( LPVOID pData, DWORD dwSize ) = 0;
    virtual HRESULT STDMETHODCALLTYPE AllocateWaveData( INT nSize ) = 0;
    virtual HRESULT STDMETHODCALLTYPE FreeWaveData() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetWaveFormat( LPVOID pFormat ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWaveFormat( LPVOID pFormat ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWaveSize() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetType( LPDWORD pdwType ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Lock( DWORD dwOffset, DWORD dwSize, LPVOID* ppBuffer1,
        LPDWORD pdwSize1, LPVOID* ppBuffer2, LPDWORD pdwSize2, DWORD dwFlags ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Unlock( LPVOID pBuffer1, DWORD dwSize1, LPVOID pBuffer2, DWORD dwSize2 ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Play( INT nPlayMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE Rewind() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetWaveTime( A3DVAL flTime ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWaveTime( LPA3DVAL pTime ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetWavePosition( DWORD dwPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWavePosition( LPDWORD pdwPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition3fv( LPA3DVAL pPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition3fv( LPA3DVAL pPosition ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOrientationAngles3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrientationAngles3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOrientationAngles3fv( LPA3DVAL pAngles ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrientationAngles3fv( LPA3DVAL pAngles ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOrientation6f( A3DVAL dirX, A3DVAL dirY, A3DVAL dirZ,
        A3DVAL upX, A3DVAL upY, A3DVAL upZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrientation6f( LPA3DVAL pDirX, LPA3DVAL pDirY, LPA3DVAL pDirZ,
        LPA3DVAL pUpX, LPA3DVAL pUpY, LPA3DVAL pUpZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOrientation6fv( LPA3DVAL pOrientation ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrientation6fv( LPA3DVAL pOrientation ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVelocity3f( A3DVAL x, A3DVAL y, A3DVAL z ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVelocity3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVelocity3fv( LPA3DVAL pVelocity ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVelocity3fv( LPA3DVAL pVelocity ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCone( A3DVAL flInnerAngle, A3DVAL flOuterAngle, A3DVAL flOuterGain ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCone( LPA3DVAL pInnerAngle, LPA3DVAL pOuterAngle, LPA3DVAL pOuterGain ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMinMaxDistance( A3DVAL flMinDistance, A3DVAL flMaxDistance,
        DWORD dwMaxDistanceMute ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMinMaxDistance( LPA3DVAL pMinDistance, LPA3DVAL pMaxDistance,
        LPDWORD pdwMaxDistanceMute ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetGain( A3DVAL flGain ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGain( LPA3DVAL pGain ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPitch( A3DVAL flPitch ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPitch( LPA3DVAL pPitch ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDopplerScale( A3DVAL flScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDopplerScale( LPA3DVAL pScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDistanceModelScale( A3DVAL flScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDistanceModelScale( LPA3DVAL pScale ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEq( A3DVAL flEq ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEq( LPA3DVAL pEq ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPriority( A3DVAL flPriority ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPriority( LPA3DVAL pPriority ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRenderMode( DWORD dwMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRenderMode( LPDWORD pdwMode ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAudibility( LPA3DVAL pAudibility ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOcclusionFactor( LPA3DVAL pFactor ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatus( LPDWORD pdwStatus ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPanValues(DWORD, LPA3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPanValues(DWORD, LPA3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetWaveEvent(DWORD, HANDLE) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearWaveEvents() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTransformMode(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTransformMode(LPDWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionDelayScale(A3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionDelayScale(LPA3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetReflectionGainScale(A3DVAL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReflectionGainScale(LPA3DVAL) = 0;

};
