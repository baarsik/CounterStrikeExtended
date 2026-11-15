#if defined(SW_RENDERER)
#include "model.h"
#else
#include "gl_model.h"
#endif
#include "common.h"
#include "model_visibility.h"
#if !defined(BSP_LOADING_BASELINE)
#include "brush_model_loading.h"
#include "loader_fixture.h"
#include "fat_fixture.h"
#include "light_trace_fixture.h"
#endif

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

static int Identity(int value) {
    return value;
}

static short IdentityShort(short value) {
    return value;
}

static float IdentityFloat(float value) {
    return value;
}

int (*LittleLong)(int) = Identity;
short (*LittleShort)(short) = IdentityShort;
float (*LittleFloat)(float) = IdentityFloat;

template <class T> static T Read(const std::vector<byte>& bytes, std::size_t offset) {
    T result;
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

template <class T> static void Put(std::vector<byte>& bytes, std::size_t offset, const T& value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

struct Fixture {
    std::vector<byte> bytes = std::vector<byte>(sizeof(dheader_t));
    dheader_t header{};

    Fixture() {
        header.version = BSPVERSION;
    }

    template <class T> void Lump(int index, const std::vector<T>& records) {
        // Deliberately leave disk records unaligned.
        bytes.push_back(0);
        header.lumps[index].fileofs = static_cast<int>(bytes.size());
        header.lumps[index].filelen = static_cast<int>(records.size() * sizeof(T));
        const auto* start = reinterpret_cast<const byte*>(records.data());
        if (!records.empty()) {
            bytes.insert(bytes.end(), start, start + records.size() * sizeof(T));
        }
        Put(bytes, 0, header);
    }

    template <class T> T Get(int lump, int index = 0) const {
        return Read<T>(bytes, header.lumps[lump].fileofs + index * sizeof(T));
    }

    template <class T> void Set(int lump, const T& record, int index = 0) {
        Put(bytes, header.lumps[lump].fileofs + index * sizeof(T), record);
    }
};

static Fixture Triangle() {
    Fixture fixture;
    fixture.Lump<byte>(LUMP_ENTITIES, {'{', '}', 0});
    dplane_t plane{};
    plane.normal[2] = 1;
    plane.type = PLANE_Z;
    fixture.Lump<dplane_t>(LUMP_PLANES, {plane});
    fixture.Lump<int>(LUMP_TEXTURES, {1, -1});
    fixture.Lump<dvertex_t>(LUMP_VERTEXES, {{{0, 0, 0}}, {{16, 0, 0}}, {{0, 16, 0}}});
    fixture.Lump<dedge_t>(LUMP_EDGES, {{{0, 1}}, {{1, 2}}, {{2, 0}}});
    fixture.Lump<int>(LUMP_SURFEDGES, {0, 1, 2});
    texinfo_t texture{};
    texture.vecs[0][0] = 1;
    texture.vecs[1][1] = 1;
    fixture.Lump<texinfo_t>(LUMP_TEXINFO, {texture});
    dface_t face{};
    face.numedges = 3;
    std::memset(face.styles, 255, sizeof(face.styles));
    face.styles[0] = 0;
    fixture.Lump<dface_t>(LUMP_FACES, {face});
    constexpr int TriangleLightBytes = 2 * 2 * 3;
    fixture.Lump<byte>(LUMP_LIGHTING, std::vector<byte>(TriangleLightBytes, 127));
    fixture.Lump<unsigned short>(LUMP_MARKSURFACES, {0});
    dleaf_t solid{};
    solid.contents = CONTENTS_SOLID;
    solid.visofs = -1;
    dleaf_t empty = solid;
    empty.contents = CONTENTS_EMPTY;
    empty.nummarksurfaces = 1;
    fixture.Lump<dleaf_t>(LUMP_LEAFS, {solid, empty});
    dnode_t node{};
    node.children[0] = -1;
    node.children[1] = -2;
    node.numfaces = 1;
    fixture.Lump<dnode_t>(LUMP_NODES, {node});
    dclipnode_t clip{};
    clip.children[0] = CONTENTS_EMPTY;
    clip.children[1] = CONTENTS_SOLID;
    fixture.Lump<dclipnode_t>(LUMP_CLIPNODES, {clip});
    dmodel_t model{};
    model.visleafs = 1;
    model.numfaces = 1;
    fixture.Lump<dmodel_t>(LUMP_MODELS, {model});
    return fixture;
}

static bool Accepted(const Fixture& fixture, bool blueShift = false) {
#if defined(BSP_LOADING_BASELINE)
    return ModelVisibility::BrushLumps(fixture.bytes.data(), static_cast<int>(fixture.bytes.size()));
#else
    const bool expected =
        BrushModelLoading::Check(fixture.bytes.data(), static_cast<int>(fixture.bytes.size()), blueShift) == nullptr;
    std::memset(mod_known, 0, sizeof(mod_known));
    loadmodel = &mod_known[0];
    std::strcpy(loadmodel->name, "fixture.bsp");
    std::strcpy(com_gamedir, blueShift ? "bshift" : "valve");
    hunkAllocations.clear();
    bool accepted = true;
    auto bytes = fixture.bytes;
    try {
        Mod_LoadBrushModel(loadmodel, bytes.data(), static_cast<int>(bytes.size()));
    } catch (const std::runtime_error&) {
        accepted = false;
        assert(hunkAllocations.empty()); // Rejection precedes all model allocations and publication.
    }
    assert(accepted == expected);
    if (accepted) {
        assert(mod_known[0].nodes->plane == mod_known[0].planes);
        assert(mod_known[0].surfaces->texinfo == mod_known[0].texinfo);
        assert(mod_known[0].leafs[1].parent == mod_known[0].nodes);
        assert(mod_known[0].nodes->parent == nullptr);
    }
    return accepted;
#endif
}

template <class T, class Change> static void RejectRecord(int lump, Change change) {
    Fixture fixture = Triangle();
    T record = fixture.Get<T>(lump);
    change(record);
    fixture.Set(lump, record);
    assert(!Accepted(fixture));
}

static Fixture EmbeddedTexture() {
    Fixture fixture = Triangle();
    constexpr int Width = 16;
    constexpr int MipBytes = 256 + 64 + 16 + 4;
    constexpr int PaletteBytes = 256 * 3;
    constexpr int DirectoryBytes = 2 * sizeof(int);
    std::vector<byte> bytes(DirectoryBytes + sizeof(miptex_t) + MipBytes + sizeof(unsigned short) + PaletteBytes);
    Put(bytes, 0, 1);
    Put(bytes, sizeof(int), DirectoryBytes);
    miptex_t texture{};
    std::strcpy(texture.name, "brick");
    texture.width = Width;
    texture.height = Width;
    int offset = sizeof(texture);
    for (int level = 0; level < MIPLEVELS; ++level) {
        texture.offsets[level] = offset;
        offset += (Width * Width) >> (level * 2);
    }
    Put(bytes, DirectoryBytes, texture);
    Put<unsigned short>(bytes, DirectoryBytes + sizeof(texture) + MipBytes, 256);
    fixture.Lump<byte>(LUMP_TEXTURES, bytes);
    return fixture;
}

static void ReviewRegressions() {
    Fixture fixture = EmbeddedTexture();
    assert(Accepted(fixture));
#if !defined(BSP_LOADING_BASELINE)
    assert(mod_known[0].textures[0]->width == 16);
    assert(mod_known[0].textures[0]->height == 16);
#endif
    const int textureOffset = fixture.header.lumps[LUMP_TEXTURES].fileofs + 2 * sizeof(int);
    auto texture = Read<miptex_t>(fixture.bytes, textureOffset);
    texture.offsets[1] = sizeof(miptex_t) + 256 + 64 + 16 + 4;
    Put(fixture.bytes, textureOffset, texture);
    assert(!Accepted(fixture));
    fixture = Triangle();
    auto leaf = fixture.Get<dleaf_t>(LUMP_LEAFS);
    leaf.contents = CONTENTS_EMPTY;
    fixture.Set(LUMP_LEAFS, leaf);
    assert(!Accepted(fixture));
    fixture = Triangle();
    auto model = fixture.Get<dmodel_t>(LUMP_MODELS);
    model.visleafs = 0;
    fixture.Set(LUMP_MODELS, model);
    assert(!Accepted(fixture));
}

static void Rejections() {
    Fixture fixture = Triangle();
    fixture.header.lumps[LUMP_VERTEXES].fileofs = static_cast<int>(fixture.bytes.size());
    Put(fixture.bytes, 0, fixture.header);
    assert(!Accepted(fixture));
    for (int index = 0; index < HEADER_LUMPS; ++index) {
        fixture = Triangle();
        fixture.header.lumps[index].filelen = -1;
        Put(fixture.bytes, 0, fixture.header);
        assert(!Accepted(fixture));
    }
    RejectRecord<dedge_t>(LUMP_EDGES, [](auto& value) {
        value.v[0] = 3;
    });
    RejectRecord<int>(LUMP_SURFEDGES, [](auto& value) {
        value = std::numeric_limits<int>::min();
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.firstedge = -1;
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.numedges = 2;
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.texinfo = -1;
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.planenum = 1;
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.lightofs = 1;
    });
    RejectRecord<dface_t>(LUMP_FACES, [](auto& value) {
        value.styles[1] = 1;
    });
    RejectRecord<texinfo_t>(LUMP_TEXINFO, [](auto& value) {
        value.miptex = -1;
    });
    RejectRecord<texinfo_t>(LUMP_TEXINFO, [](auto& value) {
        value.vecs[0][0] = INFINITY;
    });
    RejectRecord<dvertex_t>(LUMP_VERTEXES, [](auto& value) {
        value.point[0] = NAN;
    });
    RejectRecord<dvertex_t>(LUMP_VERTEXES, [](auto& value) {
        value.point[0] = 1e30f;
    });
    RejectRecord<dplane_t>(LUMP_PLANES, [](auto& value) {
        value.type = 6;
    });
    RejectRecord<unsigned short>(LUMP_MARKSURFACES, [](auto& value) {
        value = 1;
    });
    RejectRecord<dleaf_t>(LUMP_LEAFS, [](auto& value) {
        value.contents = 0;
    });
    RejectRecord<dleaf_t>(LUMP_LEAFS, [](auto& value) {
        value.firstmarksurface = 2;
    });
    RejectRecord<dnode_t>(LUMP_NODES, [](auto& value) {
        value.children[0] = -3;
    });
    RejectRecord<dnode_t>(LUMP_NODES, [](auto& value) {
        value.children[0] = 0;
    });
    RejectRecord<dclipnode_t>(LUMP_CLIPNODES, [](auto& value) {
        value.children[0] = 0;
    });
    RejectRecord<dclipnode_t>(LUMP_CLIPNODES, [](auto& value) {
        value.planenum = 1;
    });
    RejectRecord<dmodel_t>(LUMP_MODELS, [](auto& value) {
        value.headnode[0] = 1;
    });
    RejectRecord<dmodel_t>(LUMP_MODELS, [](auto& value) {
        value.headnode[1] = 2;
    });
    RejectRecord<dmodel_t>(LUMP_MODELS, [](auto& value) {
        value.visleafs = MAX_MAP_LEAFS + 1;
    });
    RejectRecord<dmodel_t>(LUMP_MODELS, [](auto& value) {
        value.firstface = -1;
    });
    fixture = Triangle();
    fixture.bytes[fixture.header.lumps[LUMP_ENTITIES].fileofs + 2] = '}';
    assert(!Accepted(fixture));
    fixture = Triangle();
    fixture.Lump<int>(LUMP_TEXTURES, {std::numeric_limits<int>::max()});
    assert(!Accepted(fixture));
    // Check disconnected components as well as the world root.
    fixture = Triangle();
    dnode_t extra = fixture.Get<dnode_t>(LUMP_NODES);
    extra.children[0] = 1;
    fixture.Lump<dnode_t>(LUMP_NODES, {fixture.Get<dnode_t>(LUMP_NODES), extra});
    assert(!Accepted(fixture));
}

static void Compatibility() {
    Fixture fixture = Triangle();
    assert(Accepted(fixture));
    fixture.header.version = Q1BSP_VERSION;
    Put(fixture.bytes, 0, fixture.header);
    assert(Accepted(fixture));
    std::swap(fixture.header.lumps[LUMP_ENTITIES], fixture.header.lumps[LUMP_PLANES]);
    Put(fixture.bytes, 0, fixture.header);
    assert(Accepted(fixture, true));
    fixture = Triangle();
    fixture.Lump<int>(LUMP_TEXTURES, {});
    assert(Accepted(fixture));
    fixture = Triangle();
    fixture.Lump<byte>(LUMP_ENTITIES, {0});
    assert(Accepted(fixture));
    fixture = Triangle();
    fixture.Lump<dleaf_t>(LUMP_LEAFS, {fixture.Get<dleaf_t>(LUMP_LEAFS), fixture.Get<dleaf_t>(LUMP_LEAFS, 1),
                                       fixture.Get<dleaf_t>(LUMP_LEAFS, 1)});
    assert(Accepted(fixture)); // Extra inline-only leaves do not enlarge the world's visibility row.
    fixture = Triangle();
    dmodel_t model = fixture.Get<dmodel_t>(LUMP_MODELS);
    model.headnode[1] = CONTENTS_EMPTY;
    fixture.Set(LUMP_MODELS, model);
    assert(Accepted(fixture));
    model.headnode[1] = fixture.header.lumps[LUMP_CLIPNODES].filelen / sizeof(dclipnode_t);
    fixture.Set(LUMP_MODELS, model);
    assert(Accepted(fixture));
#if !defined(BSP_LOADING_BASELINE)
    constexpr float WrappedOrigin = -45120.0f;
    constexpr short ExpectedOrigin = 20416;
    const float minimum[2] = {WrappedOrigin, 0};
    const float maximum[2] = {WrappedOrigin + 16, 16};
    short origins[2];
    short extents[2];
    assert(BrushModelLoading::Extents(minimum, maximum, origins, extents));
    assert(origins[0] == ExpectedOrigin && extents[0] == 16 && extents[1] == 16);
#endif
}

static void UnstyledLightSpans() {
    constexpr byte NoLightStyles = 255;
    constexpr int NoLightOffset = -1;
    Fixture fixture = Triangle();
    auto face = fixture.Get<dface_t>(LUMP_FACES);
    std::memset(face.styles, NoLightStyles, sizeof(face.styles));
    // An unstyled face can start at the end of the lighting lump without consuming samples.
    face.lightofs = fixture.header.lumps[LUMP_LIGHTING].filelen;
    fixture.Set(LUMP_FACES, face);
    assert(Accepted(fixture));

    ++face.lightofs;
    fixture.Set(LUMP_FACES, face);
    assert(!Accepted(fixture));
    face.lightofs = NoLightOffset - 1;
    fixture.Set(LUMP_FACES, face);
    assert(!Accepted(fixture));

    face.lightofs = 0;
    fixture.Set(LUMP_FACES, face);
    assert(Accepted(fixture));

    fixture.Lump<byte>(LUMP_LIGHTING, {});
    assert(Accepted(fixture));
#if !defined(BSP_LOADING_BASELINE)
    assert(loadmodel->lightdata == nullptr);
    assert(loadmodel->surfaces[0].samples == nullptr);
#endif

    face.styles[0] = 0;
    fixture.Set(LUMP_FACES, face);
    assert(!Accepted(fixture));
    face.lightofs = NoLightOffset;
    fixture.Set(LUMP_FACES, face);
    assert(Accepted(fixture));
#if !defined(BSP_LOADING_BASELINE)
    assert(loadmodel->surfaces[0].samples == nullptr);
#endif
}

#if !defined(BSP_LOADING_BASELINE)
static void UnstyledFaceBlocksLightTrace() {
    constexpr byte NoLightStyles = 255;
    constexpr int NormalLightScale = 256;
    constexpr unsigned int DarkHitRed = 1;
    constexpr unsigned int LitSample = 63;
    constexpr int SplitNode = 0;
    constexpr int UnstyledNode = 1;
    constexpr int LitNode = 2;
    Fixture fixture = Triangle();
    auto face = fixture.Get<dface_t>(LUMP_FACES);
    std::memset(face.styles, NoLightStyles, sizeof(face.styles));
    face.lightofs = fixture.header.lumps[LUMP_LIGHTING].filelen;
    fixture.Set(LUMP_FACES, face);
    assert(Accepted(fixture));

    msurface_t* originalSurfaces = loadmodel->surfaces;
    std::array<msurface_t, 2> surfaces{originalSurfaces[0], originalSurfaces[0]};
    surfaces[1].samples = loadmodel->lightdata;
    surfaces[1].styles[0] = 0;
    loadmodel->lightdata[1] = {LitSample, LitSample, LitSample};
    loadmodel->surfaces = surfaces.data();
    cl.worldmodel = loadmodel;
    d_lightstylevalue[0] = NormalLightScale;

    // Trace through a split: an unstyled face above must block the lit face below it.
    constexpr std::array<float, 3> PlaneHeights{16.0f, 24.0f, 8.0f};
    std::array<mplane_t, PlaneHeights.size()> planes{};
    std::array<mnode_t, PlaneHeights.size()> nodes{};
    mnode_t leaf{};
    leaf.contents = CONTENTS_EMPTY;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        planes[index].normal[2] = 1;
        planes[index].dist = PlaneHeights[index];
        nodes[index].plane = &planes[index];
        nodes[index].children[0] = &leaf;
        nodes[index].children[1] = &leaf;
    }
    nodes[SplitNode].children[0] = &nodes[UnstyledNode];
    nodes[SplitNode].children[1] = &nodes[LitNode];
    nodes[UnstyledNode].numsurfaces = 1;
    nodes[LitNode].firstsurface = 1;
    nodes[LitNode].numsurfaces = 1;
    vec3_t start{16.0f, 0.0f, 32.0f};
    vec3_t end{16.0f, 0.0f, -16.0f};
    colorVec color = RecursiveLightPoint(&nodes[SplitNode], start, end);
    assert(color.r == DarkHitRed && color.g == 0 && color.b == 0);

    surfaces[0].flags |= SURF_DRAWTILED;
    color = RecursiveLightPoint(&nodes[SplitNode], start, end);
    assert(color.r == LitSample && color.g == LitSample && color.b == LitSample);
    loadmodel->surfaces = originalSurfaces;
    cl.worldmodel = nullptr;
}
#endif

int main(int argc, char** argv) {
    Compatibility();
    Rejections();
    ReviewRegressions();
    UnstyledLightSpans();
#if !defined(BSP_LOADING_BASELINE)
    UnstyledFaceBlocksLightTrace();
    model_t model{};
    sv.worldmodel = &model;
    vec3_t origin{};
    model.numleafs = MAX_MAP_LEAFS;
    assert(SV_FatPVS(origin) == fatpvs);
    assert(fatbytes == static_cast<int>(sizeof(fatpvs)));
    assert(SV_FatPAS(origin) == fatpas);
    assert(fatpasbytes == static_cast<int>(sizeof(fatpas)));
#endif
    int maps = 0;
#if !defined(BSP_LOADING_BASELINE)
    if (argc > 1) {
        for (const auto& item : std::filesystem::recursive_directory_iterator(argv[1])) {
            if (!item.is_regular_file() || item.path().extension() != ".bsp") {
                continue;
            }
            std::ifstream input(item.path(), std::ios::binary);
            std::vector<byte> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
            const bool blueShift = item.path().string().find("/bshift/") != std::string::npos;
            const char* error = BrushModelLoading::Check(bytes.data(), static_cast<int>(bytes.size()), blueShift);
            if (error) {
                std::fprintf(stderr, "%s: %s\n", item.path().c_str(), error);
            }
            assert(!error);
            ++maps;
        }
    }
#endif
    std::printf("PASS BSP references, light spans, graph cycles and compatibility (%d asset maps)\n", maps);
}
