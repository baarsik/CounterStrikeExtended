#include "fixture.h"
#include "A3dGeom.h"
#include "A3dList.h"
#include "A3dWallBuilder.h"
#include "A3dOpeningBuilder.h"
#include "ChunkPage.h"
#include "dalinfo.h"
#include <cmath>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

// A labeled record tap, not a producer substitute. Every submitted complete
// record passes through the source's actual ResMan backend and DalBufferInfo
// into a deterministic native software buffer. Source samples travel through
// real stream Update/Write/ConvertPcm. No record or PCM algorithm is replaced.
// SDL dummy owns the provider transport; the resource worker is stopped in Scene.
struct RecordSink final : IA3dSourceRenderBuffer {
    IA3dSourceRenderBuffer* destination = nullptr;
    PcmOwner pcm;
    static constexpr DWORD kBytes = kSourceFrames * sizeof(SHORT);
    static constexpr DWORD kPooledStream = 2;
    a3d_render_description_t description{A3D_RENDER_WRITE_BUFFER, kBytes, kSampleRate, 16, kBytes};
    DalBufferInfo stream{nullptr, pcm.buffer, &description, kPooledStream};
    A3DCTRL_SRC_SUPER record{};
    DWORD submissions = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override {
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE SetA3dSuperCtrl(A3DCTRL_SRC_SUPER* control, DWORD bytes) override {
        CHECK(bytes == sizeof(*control));
        record = *control;
        ++submissions;
        CHECK(destination->SetA3dSuperCtrl(control, bytes) == S_OK);
        CHECK(std::memcmp(&pcm.native->voice_.control_, control, sizeof(*control)) == 0);
        return S_OK;
    }
};

struct Scene {
    CA3d* parent = CreateParent();
    CA3dSource* source = nullptr;
    RecordSink sink;
    Scene() {
        CHECK(parent->Init(nullptr, 0, 0) == S_OK);
        // Drive the actual resource owner synchronously for deterministic CPU PCM.
        dynamic_cast<ResMan*>(parent->m_pDirectSound)->StopNativeWorker();
        CHECK(parent->InitializeGeometryState() == S_OK);
        CHECK(parent->InitializeListenerState() == S_OK);
        LPA3DSOURCE created = nullptr;
        CHECK(parent->NewSource(A3DSOURCE_TYPEDEFAULT, &created) == S_OK);
        source = static_cast<CA3dSource*>(reinterpret_cast<IA3dSource202*>(created));
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * sizeof(SHORT), sizeof(SHORT), 16, 0};
        CHECK(source->SetWaveFormat(&format) == S_OK);
        CHECK(source->AllocateWaveData(kSourceFrames * sizeof(SHORT)) == S_OK);
        auto* stream = dynamic_cast<ResManBuffer*>(source->m_pBuffer);
        CHECK(stream && stream->SetStream(&sink.stream) == S_OK);
        void* data = nullptr;
        DWORD bytes = 0;
        CHECK(source->Lock(0, RecordSink::kBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        std::memcpy(data, sink.pcm.native->samples_->data, bytes);
        CHECK(source->Unlock(data, bytes, nullptr, 0) == S_OK);
        CHECK(sink.pcm.buffer->Lock(0, RecordSink::kBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        std::memset(data, 0, bytes);
        CHECK(sink.pcm.buffer->Unlock(data, bytes, nullptr, 0) == S_OK);
        sink.destination = source->m_pSourceBackend;
        CHECK(sink.destination);
        source->m_pSourceBackend = &sink;
        CHECK(source->SetPosition3f(0, 0, 2) == S_OK);
        CHECK(source->SetGain(0.75f) == S_OK);
        CHECK(source->Play(A3D_LOOPED) == S_OK);
        parent->m_dwUpdateTickPeriod = 0;
        parent->m_dwReflectionUpdateModulus = 1;
        parent->m_dwOcclusionUpdateModulus = 1;
        parent->m_flFrameTime = 0.5f;
    }

    ~Scene() {
        CHECK(source->Stop() == S_OK);
        CHECK(dynamic_cast<ResManBuffer*>(source->m_pBuffer)->SetStream(nullptr) == S_OK);
        source->m_pSourceBackend = sink.destination;
        source->Release();
        CHECK(parent->Release() == 0);
    }

    A3DCTRL_SRC_SUPER Render() {
        const DWORD before = sink.submissions;
        parent->RenderSources();
        CHECK(sink.submissions == before + 1);
        auto* stream = dynamic_cast<ResManBuffer*>(source->m_pBuffer);
        CHECK(stream && std::memcmp(&stream->m_nativeControl, &sink.record, sizeof(sink.record)) == 0);
        CHECK(stream->Update(0) == S_OK);
        CHECK(std::memcmp(sink.pcm.native->samples_->data, stream->m_nativeSamples->data,
            RecordSink::kBytes) == 0);
        return sink.record;
    }

    void Wall(float z, DWORD tag = 17) {
        constexpr float kExtent = 10;
        CHECK(parent->Begin(A3D_QUADS) == S_OK);
        CHECK(parent->Tag(tag) == S_OK);
        CHECK(parent->Normal3f(0, 0, 1) == S_OK);
        CHECK(parent->Vertex3f(-kExtent, -kExtent, z) == S_OK);
        CHECK(parent->Vertex3f(kExtent, -kExtent, z) == S_OK);
        CHECK(parent->Vertex3f(kExtent, kExtent, z) == S_OK);
        CHECK(parent->Vertex3f(-kExtent, kExtent, z) == S_OK);
        CHECK(parent->End() == S_OK);
    }

    void Reflections() {
        // Producer qualification only. Public native reflection negotiation must
        // reject this bit; direct producer inspection is not acoustic support.
        parent->m_dwEnabledFeatures |= A3D_1ST_REFLECTIONS;
        source->m_dwRenderMode |= A3DSOURCE_RENDERMODE_1ST_REFLECTIONS;
        source->m_bWithinReflectionRange = TRUE;
    }
};

int Active(const A3DCTRL_SRC_SUPER& record) {
    int count = 0;
    for (const auto& reflection : record.Reflections) {
        count += reflection.dwEnabled && reflection.dwActive;
    }
    return count;
}

void SourcePcm() {
    Scene baseline;
    const auto record = baseline.Render();
    CHECK(record.flPitch == 1 && record.Left.flGain > 0 && record.Right.flGain > 0);
    const auto pcm = baseline.sink.pcm.Render();
    CHECK(std::any_of(pcm.begin(), pcm.end(), [](SHORT value) { return value != 0; }));
    for (int change = 0; change < 3; ++change) {
        Scene altered;
        if (change == 0) {
            CHECK(altered.source->SetGain(0) == S_OK);
        } else if (change == 1) {
            CHECK(altered.source->SetPitch(0.5f) == S_OK);
        } else {
            CHECK(altered.source->SetPosition3f(2, 0, 2) == S_OK);
        }
        const auto changed = altered.Render();
        CHECK(std::memcmp(&changed, &record, sizeof(record)) != 0);
        const auto result = altered.sink.pcm.Render();
        CHECK(result != pcm);
        if (change == 0) {
            CHECK(changed.Left.flGain == 0 && changed.Right.flGain == 0);
            CHECK(std::all_of(result.begin(), result.end(), [](SHORT value) { return value == 0; }));
        } else if (change == 1) {
            CHECK(changed.flPitch == 0.5f);
            CHECK(altered.sink.pcm.native->voice_.voice_.lRateRequested !=
                baseline.sink.pcm.native->voice_.voice_.lRateRequested);
        } else {
            CHECK(changed.Left.flAzimuth != record.Left.flAzimuth);
        }
    }
}

void GeometryRecords(bool history) {
    Scene scene;
    scene.Reflections();
    CHECK(Active(scene.Render()) == 0);
    scene.Wall(-2);
    const auto first = scene.Render();
    CHECK(Active(first) == 1);
    const auto* node = scene.source->m_pReflections[scene.source->m_iReflectionBuffer];
    CHECK(node && node->iSlot >= 0 && node->bNewSlot);
    const int slot = node->iSlot;
    CHECK(first.Reflections[slot].Left.flDelay > 0);
    if (history) {
        const auto second = scene.Render();
        CHECK(Active(second) == 1 && !second.Reflections[slot].dwNewSlot);
        CHECK(scene.source->m_pReflections[scene.source->m_iReflectionBuffer]->iSlot == slot);
        CHECK(scene.parent->Clear() == S_OK);
        scene.Wall(-2, 23);
        CHECK(scene.Render().Reflections[slot].dwNewSlot);
    } else {
        CHECK(scene.parent->Clear() == S_OK);
        scene.Wall(-4);
        const auto distant = scene.Render();
        CHECK(Active(distant) == 1);
        CHECK(distant.Reflections[slot].Left.flDelay > first.Reflections[slot].Left.flDelay);
        CHECK(distant.Reflections[slot].Left.flGain < first.Reflections[slot].Left.flGain);
        CHECK(scene.parent->Clear() == S_OK);
        CA3dMaterial material;
        CHECK(material.SetReflectance(0.25f, 0.5f) == S_OK);
        CHECK(scene.parent->BindMaterial(&material) == S_OK);
        scene.Wall(-4);
        const auto attenuated = scene.Render();
        CHECK(Active(attenuated) == 1);
        CHECK(attenuated.Reflections[slot].Left.flGain < distant.Reflections[slot].Left.flGain);
        CHECK(attenuated.Reflections[slot].flEq != distant.Reflections[slot].flEq);
        CHECK(attenuated.Left.flGain == distant.Left.flGain);
    }
    CHECK(scene.parent->Clear() == S_OK);
    CHECK(Active(scene.Render()) == 0);
}

void Occlusion() {
    Scene clear;
    const auto direct = clear.Render();
    const auto pcm = clear.sink.pcm.Render();
    Scene blocked;
    CHECK(blocked.parent->Enable(A3D_OCCLUSIONS) == S_OK);
    CHECK(blocked.source->SetRenderMode(blocked.source->m_dwRenderMode | A3DSOURCE_RENDERMODE_OCCLUSIONS) == S_OK);
    CA3dMaterial material;
    CHECK(material.SetTransmittance(0.25f, 0.5f) == S_OK);
    CHECK(blocked.parent->BindMaterial(&material) == S_OK);
    blocked.Wall(1);
    const auto occluded = blocked.Render();
    CHECK(occluded.Left.flGain < direct.Left.flGain);
    CHECK(occluded.Right.flGain < direct.Right.flGain);
    CHECK(occluded.flEq != direct.flEq);
    CHECK(blocked.sink.pcm.Render() != pcm);
    PcmOwner eqChanged;
    PcmOwner expected;
    expected.Control(occluded);
    auto changed = occluded;
    changed.flEq = direct.flEq;
    eqChanged.Control(changed);
    CHECK(eqChanged.Render() == expected.Render());
}

constexpr A3DVECTOR kOrigin{0, 0, 0, 1};
constexpr A3DVECTOR kForward{0, 0, 1, 0};
constexpr A3DVECTOR kUp{0, 1, 0, 0};

void BuildQuad(CA3dGeometryBuilder& builder) {
    CHECK(builder.Begin(A3D_QUADS) == S_OK);
    CHECK(builder.AddVertex3f(-2, -2, 0) == S_OK);
    CHECK(builder.AddVertex3f(2, -2, 0) == S_OK);
    CHECK(builder.AddVertex3f(2, 2, 0) == S_OK);
    CHECK(builder.AddVertex3f(-2, 2, 0) == S_OK);
    CHECK(builder.End() == S_OK);
    CHECK(builder.Validate() == S_OK);
}

void MaterialOpeningTransforms() {
    Scene scene;
    CA3dWallBuilder wall(A3D_SCENE_3D);
    BuildQuad(wall);
    A3DVECTOR translation{3, 4, -2, 1};
    CA3dGeometryFace face(&wall, 0, translation, kForward, kUp);
    auto* material = new CA3dMaterial;
    material->AddRef();
    CHECK(material->SetReflectance(0.75f, 0.5f) == S_OK);
    CHECK(face.SetMaterial(material) == S_OK);
    material->Release();
    face.Render(scene.parent);
    auto* commands = scene.parent->m_pCommandBuffer->m_pCommands;
    CHECK(scene.parent->m_pCommandBuffer->m_cCommands == 2);
    CHECK(commands[0].m_dwOpcode == A3D_MATERIAL);
    CHECK(commands[0].m_Payload.m_MaterialProperties[0] == 0.75f);
    CHECK(commands[0].m_Payload.m_MaterialProperties[1] == material->m_flReflectionFilter);
    CHECK(material->m_flReflectionFilter >= 0 && material->m_flReflectionFilter <= 1);
    CHECK(material->m_flReflectionFilter != 0.5f);
    CHECK(std::memcmp(commands[0].m_Payload.m_MaterialProperties, &material->m_flReflectance,
        sizeof(commands[0].m_Payload.m_MaterialProperties)) == 0);
    CHECK(commands[1].m_Payload.m_Primitive.m_Vertices[0][0] == 1);
    CHECK(commands[1].m_Payload.m_Primitive.m_Vertices[0][1] == 2);
    CHECK(commands[1].m_Payload.m_Primitive.m_Vertices[0][2] == -2);
    CA3dOpeningBuilder builder(A3D_SCENE_3D);
    BuildQuad(builder);
    CA3dOpening opening(&builder, translation, kForward, kUp);
    CHECK(opening.SetOpeningFactor(0.25f) == S_OK);
    opening.Render(scene.parent);
    CHECK(commands[2].m_dwOpcode == A3D_SUB_QUADS);
    CHECK(commands[2].m_pOpeningFactor == &opening.m_flOpeningFactor);
    CHECK(*commands[2].m_pOpeningFactor == 0.25f);
    CHECK(commands[2].m_Payload.m_Primitive.m_Vertices[0][2] == -2);
    CHECK(opening.SetOpeningFactor(0.75f) == S_OK);
    CHECK(*commands[2].m_pOpeningFactor == 0.75f);
    // Commands borrow the live opening factor; clear before owner destruction.
    CHECK(scene.parent->Clear() == S_OK);

    Scene passage;
    CHECK(passage.parent->Enable(A3D_OCCLUSIONS) == S_OK);
    CHECK(passage.source->SetRenderMode(passage.source->m_dwRenderMode | A3DSOURCE_RENDERMODE_OCCLUSIONS) == S_OK);
    A3DVECTOR blockingPosition{0, 0, 1, 1};
    CA3dGeometryFace blockingFace(&wall, 0, blockingPosition, kForward, kUp);
    CA3dOpening aperture(&builder, blockingPosition, kForward, kUp);
    blockingFace.Render(passage.parent);
    aperture.Render(passage.parent);
    CHECK(aperture.SetOpeningFactor(1) == S_OK);
    const auto open = passage.Render();
    CHECK(aperture.SetOpeningFactor(0.75f) == S_OK);
    const auto partial = passage.Render();
    std::printf("opening records: full gain=%g eq=%g; partial gain=%g eq=%g factor=%g\n",
        open.Left.flGain, open.flEq, partial.Left.flGain, partial.flEq, passage.source->m_flOcclusionFactor);
    // Retained RenderSources continues scanning at factor 1, then applies the
    // solid-wall material at end-of-commands. This observed source behavior is
    // not a claim of correct OEM opening acoustics; full OEM opening acoustics remain unqualified.
    CHECK(open.Left.flGain == 0 && open.flEq == 0.99f);
    CHECK(partial.Left.flGain > open.Left.flGain && partial.flEq < open.flEq);
    CHECK(passage.source->m_flOcclusionFactor == 0.25f);
    CHECK(passage.parent->Clear() == S_OK);
}

void HighAddressDomains() {
    Scene scene;
    scene.Reflections();
    CA3dWallBuilder wall(A3D_SCENE_3D);
    BuildQuad(wall);
    CA3dGeometryFace face(&wall, 0, kOrigin, kForward, kUp);
    face.Clear();
    constexpr std::uintptr_t kStride = UINT64_C(1) << 32;
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const std::size_t bytes = kStride + page;
    void* mapping = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(mapping != MAP_FAILED);
    auto* first = static_cast<a3d_polygon_t*>(mapping);
    auto* second = reinterpret_cast<a3d_polygon_t*>(reinterpret_cast<std::uintptr_t>(mapping) + kStride);
    CHECK(mprotect(first, page, PROT_READ | PROT_WRITE) == 0);
    CHECK(mprotect(second, page, PROT_READ | PROT_WRITE) == 0);
    const auto* original = static_cast<a3d_polygon_t*>(wall.m_Polygons.m_pFirst->m_pObject);
    *first = *original;
    *second = *original;
    for (auto* polygon : {first, second}) {
        for (auto& vertex : polygon->m_Vertices) {
            vertex[2] = -2;
        }
        face.m_Polygons.Append(polygon);
    }
    const auto address = reinterpret_cast<std::uintptr_t>(first);
    CHECK(address > UINT32_MAX && static_cast<DWORD>(address) ==
        static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(second)));
    std::printf("real polygon addresses: %p %p\n", static_cast<void*>(first), static_cast<void*>(second));
    face.Render(scene.parent);
    scene.Wall(-2, static_cast<DWORD>(address));
    const auto record = scene.Render();
    CHECK(Active(record) == 3);
    CHECK(Active(scene.Render()) == 3);
    face.m_Polygons.Clear(); // Sparse mapping owns these; the face only borrowed them.
    CHECK(scene.parent->Clear() == S_OK);
    CHECK(munmap(mapping, bytes) == 0);
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string name = argv[1];
    if (name == "source_control_pcm") {
        SourcePcm();
    } else if (name == "geometry_producer_records") {
        GeometryRecords(false);
    } else if (name == "reflection_slot_history") {
        GeometryRecords(true);
    } else if (name == "occlusion_direct_gain") {
        Occlusion();
    } else if (name == "material_opening_transforms") {
        MaterialOpeningTransforms();
    } else {
        CHECK(name == "high_address_polygon_domains");
        HighAddressDomains();
    }
    std::printf("PASS %s\n", argv[1]);
}
