#pragma once

// Declarations for the exact extracted methods. No COM/device implementation is
// supplied: the fixture exercises geometry and reflection state in isolation.
#include "LinkList.h"
#ifndef CSE_A3D_REFERENCE
#include "native_geometry_identity.h"
#endif

class CA3dSource;
class CA3d;
using IA3dGeomTarget = CA3d;
class CA3d {
public:
    // The identity component owns this parent on the stack. Mirror only the
    // retained-list reference balance; real module deletion is tested elsewhere.
    ULONG fixtureReferences = 1;
    ULONG AddRef() {
        return ++fixtureReferences;
    }
    ULONG Release() {
        return --fixtureReferences;
    }
    HRESULT STDMETHODCALLTYPE Begin(DWORD);
    HRESULT STDMETHODCALLTYPE End();
    HRESULT STDMETHODCALLTYPE Tag(DWORD);
    HRESULT STDMETHODCALLTYPE Clear();
    BOOL m_bGeometryInitialized = TRUE;
#if !defined(CSE_A3D_REFERENCE) && UINTPTR_MAX > UINT32_MAX
    void TagOwnedPolygon(const a3d_polygon_t*);
    A3dGeometryIdentity m_PrimitiveIdentity{};
#endif
    HRESULT STDMETHODCALLTYPE Vertex3fv(LPA3DVAL);
    HRESULT STDMETHODCALLTYPE Vertex3f(A3DVAL, A3DVAL, A3DVAL);
    HRESULT STDMETHODCALLTYPE Normal3fv(LPA3DVAL);
    HRESULT STDMETHODCALLTYPE Normal3f(A3DVAL, A3DVAL, A3DVAL);
    HRESULT STDMETHODCALLTYPE GetOutputGain(LPA3DVAL);
    void NextFreeChunk();
    void AppendGeometryPool(void*);
    void RenderSourceGeometry(CA3dSource*);
    double ReflectPoint(const A3DVAL*, const A3DVAL*, const A3DVAL*, A3DVAL*, A3DVAL*);
    void BindMaterial(void*);
    CA3dObjectList m_GeometryPools{};
    CA3dCommandBuffer* m_pCommandBuffer = nullptr;
    a3d_geometry_command_t* m_pCurrentCommand = nullptr;
    a3d_primitive_t m_Primitive{};
    DWORD m_nCommandCapacity = 8;
    LONG m_nNextVertex = 0;
    BOOL m_bCalculateNormal = FALSE;
    BOOL m_bCompat1009 = TRUE;
    DWORD m_dwInputMode = 0;
    float m_Matrix[1][16]{};
    int m_nMatrix = 0;
    float m_flReflectionGainScale = 1;
    float m_flOutputGain = 1;
    DWORD m_dwCompatValue1007 = 0;
    float m_flReflectionDelayScale = 1;
    float m_flMaxReflectionDelayTime = 1;
    float m_flReflectionPlaneScale = 2;
    float m_vWorldListenerPosition[3]{0, 0, 1};
    float m_MaterialProperties[6]{1, 1, 0, 0, 0, 0};
    LONG m_nRemainingGeometryPools = 0;
    DWORD m_dwPolygonTestCount = 0;
    DWORD m_dwPolygonTestLimit = 512;
};

class CA3dTransform {
public:
    float m_Matrix[16]{};
    CA3dTransform* m_pParent = nullptr;
    void TransformPolygon(a3d_polygon_t*, const a3d_polygon_t*);
    void TransformVector(A3DVECTOR);
};
class CA3dGeometryInstance {
public:
    CA3dGeometryInstance();
    ~CA3dGeometryInstance();
    void Clear();
    void CopyPolygons(CA3dObjectList*);
    CA3dObjectList m_Polygons;
};
class CA3dOpening {
public:
    void Render(CA3d*);
};
class CA3dGeometryFace : public CA3dGeometryInstance {
public:
    void Render(CA3d*);
    DWORD m_dwFlags = 0;
    void* m_pMaterial = nullptr;
    CA3dObjectList m_Openings{};
    CA3dTransform m_Transform;
};
class CA3dList {
public:
    CA3dList(CA3d*);
    virtual ~CA3dList();
    HRESULT STDMETHODCALLTYPE Begin();
    HRESULT STDMETHODCALLTYPE End();
    HRESULT STDMETHODCALLTYPE Call();
    ULONG m_cReferences;
    CA3d* m_pOwner;
    CA3dCommandBuffer* m_pCommands;
    CA3dCommandBuffer* m_pBeginBuffer;
    int m_nBeginCommand;
    CA3dCommandBuffer* m_pEndBuffer;
    int m_nEndCommand;
};
class CA3dScene {
public:
    BOOL STDMETHODCALLTYPE GetNextVertex(A3DVAL*, INT*, DWORD*);
    BOOL m_bValid = TRUE;
    CA3dList* m_pList = nullptr;
    INT m_nReadVertex = 0;
    a3d_primitive_t* m_pReadPolygon = nullptr;
};
class CA3dSource {
public:
    A3dSourceReflection* AllocateReflection();
    A3dSourceReflection* NextReflection(A3dSourceReflection*);
    void BeginReflectionFrame();
    void AddReflection(DWORD, const float[3], float, const float[6]);
#if !defined(CSE_A3D_REFERENCE) && UINTPTR_MAX > UINT32_MAX
    void AddGeometryReflection(A3dGeometryIdentity, DWORD, const float[3], float, const float[6]);
#endif
    void UpdateReflections(int);
    void ReuseReflections();
    void PruneReflections(int);
    HRESULT STDMETHODCALLTYPE InitializeReflectionSlots(A3dSourceReflection**);
    void ComputeDistanceGain(float, float*);
    void ComputeDistanceEq(float, float*);
    A3dSourceReflection* m_pReflections[2]{};
    DWORD m_nReflections[2]{};
    DWORD m_iReflectionBuffer = 0;
    CA3d* m_pA3d = nullptr;
    float m_vRenderPosition[4]{0, 0, 1, 0};
    float m_flEqFactor = 1;
    float m_flGain = 1;
    float m_flOcclusion[2]{1, 1};
    float m_flEarDistance[2]{};
    float m_flMinDistance = 1;
    float m_flMaxDistance = 100;
    float m_flDistanceModelScale = 1;
    DWORD m_dwMaxDistanceMute = 0;
    A3DCTRL_SRC_SUPER* m_pRenderState = nullptr;
    A3DCTRL_SRC_SUPER* m_pRenderStates[2]{};
    DWORD m_iRenderState = 0;
};
void __cdecl Vector_ToSphericalRadians(const float[3], float[3]);
BOOL A3dIntersectQuad(const A3DVAL*, const A3DVAL*, a3d_primitive_t*, BOOL, BOOL);
