#include "fixture_platform.h"
#include "production_bodies.inc"

static void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void IdentityMatrix(float matrix[16]) {
    std::memset(matrix, 0, sizeof(float) * 16);
    constexpr int kDimension = 4;
    for (int i = 0; i < kDimension; ++i) {
        matrix[i * kDimension + i] = 1;
    }
}

class GeometryFixture {
public:
    CA3d geometry;
    CA3dSource source;
    A3DCTRL_SRC_SUPER output{};

    explicit GeometryFixture(int capacity = 8) {
        IdentityMatrix(geometry.m_Matrix[0]);
        geometry.m_nCommandCapacity = static_cast<DWORD>(capacity);
        geometry.m_pCommandBuffer = new CA3dCommandBuffer(capacity, 0);
        geometry.m_GeometryPools.Append(geometry.m_pCommandBuffer);
        source.m_pA3d = &geometry;
        source.m_pRenderState = &output;
    }

    ~GeometryFixture() {
        Check(geometry.fixtureReferences == 1, "all recorded lists released their parent");
        // Existing frame recycling owns/frees reflection nodes. Exercise it twice
        // rather than introducing a different cleanup contract in this fixture.
        source.BeginReflectionFrame();
        source.BeginReflectionFrame();
        geometry.m_GeometryPools.m_pCurrent = geometry.m_GeometryPools.m_pFirst;
        while (auto* buffer = static_cast<CA3dCommandBuffer*>(geometry.m_GeometryPools.Next())) {
            if (!buffer->m_iBufferType) {
                delete buffer;
            }
        }
        geometry.m_GeometryPools.Clear();
    }

    void ClearCommands() {
        Check(geometry.m_GeometryPools.m_nCount == 1, "single writable pool for command reset");
        geometry.m_pCommandBuffer->m_iCursor = 0;
        geometry.m_pCommandBuffer->m_cCommands = 0;
    }

    void Render(CA3dGeometryFace& face) {
        face.Render(&geometry);
    }

    void Reflect() {
        geometry.RenderSourceGeometry(&source);
    }

    void PublicTriangle(DWORD tag) {
        geometry.Begin(A3D_TRIANGLES);
        geometry.Tag(tag);
        geometry.Normal3f(0, 0, 1);
        geometry.Vertex3f(0, 0, 0);
        geometry.Vertex3f(1, 0, 0);
        geometry.Vertex3f(0, 1, 0);
        geometry.End();
    }
};

static void InitializePolygon(a3d_polygon_t* polygon) {
    std::memset(polygon, 0, sizeof(*polygon));
    polygon->m_nVertices = A3D_TRIANGLES;
    polygon->m_Normal[2] = 1;
    polygon->m_Vertices[1][0] = 1;
    polygon->m_Vertices[2][1] = 1;
    for (int i = 0; i < A3D_TRIANGLES; ++i) {
        polygon->m_Vertices[i][3] = 1;
    }
}

class AddressPair {
public:
    a3d_polygon_t* first = nullptr;
    a3d_polygon_t* second = nullptr;
    std::size_t pageBytes = 0;
    std::size_t mappingBytes = 0;

    AddressPair() {
#if UINTPTR_MAX > UINT32_MAX
        // Reserve sparse address space, then commit only two pages 4 GiB
        // apart. This does not allocate/initialize 4 GiB of physical storage.
        constexpr std::uintptr_t kAddressStride = UINT64_C(1) << 32;
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageBytes = info.dwPageSize;
        mappingBytes = kAddressStride + pageBytes;
        first = static_cast<a3d_polygon_t*>(VirtualAlloc(nullptr, mappingBytes, MEM_RESERVE, PAGE_NOACCESS));
        Check(first != nullptr, "reserve sparse virtual address range");
        second = reinterpret_cast<a3d_polygon_t*>(reinterpret_cast<std::uintptr_t>(first) + kAddressStride);
        Check(VirtualAlloc(first, pageBytes, MEM_COMMIT, PAGE_READWRITE) == first,
            "commit first polygon page");
        Check(VirtualAlloc(second, pageBytes, MEM_COMMIT, PAGE_READWRITE) == second,
            "commit second polygon page");
#else
        pageBytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        mappingBytes = kAddressStride + pageBytes;
        first = static_cast<a3d_polygon_t*>(mmap(nullptr, mappingBytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));
        Check(first != MAP_FAILED, "reserve sparse virtual address range");
        second = reinterpret_cast<a3d_polygon_t*>(reinterpret_cast<std::uintptr_t>(first) + kAddressStride);
        Check(mprotect(first, pageBytes, PROT_READ | PROT_WRITE) == 0, "commit first polygon page");
        Check(mprotect(second, pageBytes, PROT_READ | PROT_WRITE) == 0, "commit second polygon page");
#endif
        const auto firstAddress = reinterpret_cast<std::uintptr_t>(first);
        const auto secondAddress = reinterpret_cast<std::uintptr_t>(second);
        Check(first && second && firstAddress > UINT32_MAX && secondAddress > UINT32_MAX,
            "both real allocations exceed DWORD range");
        Check(firstAddress != secondAddress && static_cast<DWORD>(firstAddress) == static_cast<DWORD>(secondAddress),
            "real allocations have distinct high bits and identical low32");
        std::printf("real polygon addresses: %p %p; low32=%08x\n", static_cast<void*>(first),
            static_cast<void*>(second), static_cast<unsigned>(firstAddress));
#else
        first = static_cast<a3d_polygon_t*>(::operator new(sizeof(a3d_polygon_t)));
        second = static_cast<a3d_polygon_t*>(::operator new(sizeof(a3d_polygon_t)));
#endif
        InitializePolygon(first);
        InitializePolygon(second);
    }

    void Release() {
        if (!first) {
            return;
        }
#if UINTPTR_MAX > UINT32_MAX
#if defined(_WIN32)
        VirtualFree(first, 0, MEM_RELEASE);
#else
        munmap(first, mappingBytes);
#endif
#else
        ::operator delete(first);
        ::operator delete(second);
#endif
        first = nullptr;
        second = nullptr;
    }

    ~AddressPair() {
        Release();
    }
};

static void Attach(CA3dGeometryFace& face, a3d_polygon_t* polygon) {
    IdentityMatrix(face.m_Transform.m_Matrix);
    face.m_Polygons.Append(polygon);
}

static void DetachBorrowed(CA3dGeometryFace& face) {
    face.m_Polygons.Clear(); // The mmap pair owns these two polygons.
}

static int Audible(const CA3dSource& source) {
    int count = 0;
    for (auto* node = source.m_pReflections[source.m_iReflectionBuffer]; node; node = node->pNext) {
        count += node->flAudibility > 0;
    }
    return count;
}

static void Run(const std::string& mode) {
    static_assert(sizeof(a3d_primitive_t) == 96, "fixed primitive payload");
    static_assert(sizeof(A3DCTRL_SRC_SUPER) == 1036, "provider control payload remains fixed");
    static_assert(offsetof(A3dSourceReflection, dwTag) == sizeof(void*), "public DWORD stays after native next pointer");
    static_assert(sizeof(a3d_geometry_command_t) == (sizeof(void*) == 4 ? 108 : 120), "existing command payload size");
    static_assert(sizeof(A3dSourceReflection) == (sizeof(void*) == 4 ? 84 : 88), "existing reflection record size");
    GeometryFixture fixture(mode == "recorded-list-multipage" ? 2 : 8);
    AddressPair pair;
    CA3dGeometryFace face;
    Attach(face, pair.first);
    if (mode == "collision" || mode == "previous-frame" || mode == "slots" || mode == "cycles" ||
        mode == "recorded-list" || mode == "recorded-list-multipage") {
        Attach(face, pair.second);
    }
    if (mode == "public-duplicates" || mode == "sort-long") {
        const int count = mode == "sort-long" ? MAX_A3D_SOURCE_REFLECTIONS : 2;
        for (int i = 0; i < count; ++i) {
            fixture.PublicTriangle(mode == "sort-long" ? static_cast<DWORD>(i) : UINT32_MAX);
        }
        fixture.Reflect();
        fixture.source.PruneReflections(1);
        Check(Audible(fixture.source) == (mode == "sort-long" ? MAX_A3D_SOURCE_REFLECTIONS : 1),
            "public identity exactness and long-sort branch");
        DetachBorrowed(face);
        return;
    } else if (mode == "public-domain") {
        fixture.Render(face);
        fixture.PublicTriangle(static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(pair.first)));
    } else if (mode == "duplicate") {
        fixture.Render(face);
        fixture.Render(face);
    } else if (mode == "recorded-list" || mode == "recorded-list-multipage" || mode == "opcode") {
        CA3dList list(&fixture.geometry);
        Check(list.Begin() == S_OK, "list begin");
        fixture.Render(face);
        if (mode == "recorded-list-multipage") {
            fixture.PublicTriangle(17);
        }
        const int commands = mode == "opcode" ? 1 : (mode == "recorded-list-multipage" ? 3 : 2);
        Check(list.End() == commands, "list stores all captured commands");
        auto* firstPool = static_cast<CA3dCommandBuffer*>(fixture.geometry.m_GeometryPools.m_pFirst->m_pObject);
        Check(std::memcmp(&list.m_pCommands->m_pCommands[0], &firstPool->m_pCommands[0],
            sizeof(a3d_geometry_command_t)) == 0, "recorded payload bytes unchanged");
        Check(fixture.geometry.Clear() == S_OK, "actual Clear releases temporary pools before list replay");
        if (mode == "opcode") {
            CA3dScene scene;
            scene.m_pList = &list;
            DWORD tag = UINT32_MAX;
            INT index = -1;
            float vertex[3]{};
            for (int i = 0; i < A3D_TRIANGLES; ++i) {
                Check(scene.GetNextVertex(vertex, &index, &tag) && index == i, "actual query vertex order");
                Check(tag == A3D_TRIANGLES, "inherited GetNextVertex returns opcode through tag argument");
            }
            Check(!scene.GetNextVertex(vertex, &index, &tag), "query exhaustion");
            DetachBorrowed(face);
            return;
        }
        // The recorded identity remains meaningful without dereferencing the
        // original polygon, matching the old address-key lifetime contract.
        DetachBorrowed(face);
        pair.Release();
        list.Call();
        fixture.Reflect();
        fixture.source.PruneReflections(1);
        Check(Audible(fixture.source) == commands, "recorded list retains distinct identities after polygons are unmapped");
        // Remove the borrowed list pool before the list destructor releases it.
        auto* node = fixture.geometry.m_GeometryPools.m_pLast;
        node->m_pPrevious->m_pNext = nullptr;
        fixture.geometry.m_GeometryPools.m_pLast = node->m_pPrevious;
        --fixture.geometry.m_GeometryPools.m_nCount;
        ::operator delete(node);
        return;
    } else if (mode == "external-slots") {
        A3dSourceReflection records[MAX_A3D_SOURCE_REFLECTIONS]{};
        A3dSourceReflection* pointers[MAX_A3D_SOURCE_REFLECTIONS]{};
        for (int i = 0; i < MAX_A3D_SOURCE_REFLECTIONS; ++i) {
            records[i].dwTag = UINT32_MAX;
            records[i].dwReserved = 0x1234;
            pointers[i] = &records[i];
        }
        Check(fixture.source.InitializeReflectionSlots(nullptr) == E_INVALIDARG, "external slots null argument");
        Check(fixture.source.InitializeReflectionSlots(pointers) == S_OK, "actual external slot initialization");
        for (int i = 0; i < MAX_A3D_SOURCE_REFLECTIONS; ++i) {
            Check(records[i].iSlot == i && records[i].dwTag == 0 && records[i].dwReserved == 0x1234,
                "caller-owned records retain layout and unaffected fields");
        }
        DetachBorrowed(face);
        return;
    } else if (mode == "copy-delete") {
        CA3dGeometryFace clone;
        IdentityMatrix(clone.m_Transform.m_Matrix);
        clone.CopyPolygons(&face.m_Polygons);
        Check(clone.m_Polygons.m_pFirst->m_pObject != pair.first, "actual polygon copy has distinct address identity");
        fixture.Render(face);
        fixture.Render(clone);
        clone.Clear();
        fixture.Reflect();
        fixture.source.PruneReflections(1);
        Check(Audible(fixture.source) == 2, "copied identity remains distinct after clone deletion");
        DetachBorrowed(face);
        return;
    } else {
        fixture.Render(face);
    }
    fixture.Reflect();
    fixture.source.PruneReflections(1);
    int expected = mode == "duplicate" ? 1 : (mode == "public-domain" || face.m_Polygons.m_nCount == 2 ? 2 : 1);
#if UINTPTR_MAX == UINT32_MAX
    if (mode == "public-domain") {
        expected = 1; // Preserve the original Win32 shared DWORD identity domain.
    }
#endif
    Check(Audible(fixture.source) == expected, "independent identities remain audible; duplicates suppress exactly one");
    if (mode == "replacement") {
        fixture.source.BeginReflectionFrame();
        fixture.ClearCommands();
        DetachBorrowed(face);
        Attach(face, pair.second);
        fixture.Render(face);
        fixture.Reflect();
        fixture.source.PruneReflections(1);
        Check(Audible(fixture.source) == 1, "replacement polygon remains audible");
        Check(fixture.source.m_pReflections[fixture.source.m_iReflectionBuffer]->bNewSlot,
            "equal-low32 different full address is a new identity in the next frame");
    }
    if (mode == "previous-frame" || mode == "slots" || mode == "cycles") {
        auto* oldFirst = fixture.source.m_pReflections[fixture.source.m_iReflectionBuffer];
        auto* oldSecond = oldFirst->pNext;
        const int firstSlot = oldFirst->iSlot;
        const int secondSlot = oldSecond->iSlot;
        Check(firstSlot != secondSlot, "different polygons occupy different slots");
        const int cycles = mode == "cycles" ? 256 : 1;
        for (int i = 0; i < cycles; ++i) {
            fixture.source.BeginReflectionFrame();
            fixture.Reflect();
            fixture.source.PruneReflections(1);
            auto* current = fixture.source.m_pReflections[fixture.source.m_iReflectionBuffer];
            Check(current->iSlot == firstSlot && current->pNext->iSlot == secondSlot,
                "previous-frame slots match full identity");
            Check(!current->bNewSlot && !current->pNext->bNewSlot, "matching identity reuses both prior slots");
        }
        if (mode == "slots") {
            fixture.source.BeginReflectionFrame();
            fixture.ClearCommands();
            // Public tag numerically equal to old polygon low bits is a new
            // domain and must not borrow either polygon's prior slot identity.
            fixture.PublicTriangle(static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(pair.first)));
            fixture.Reflect();
            fixture.source.PruneReflections(1);
            const bool newSlot = fixture.source.m_pReflections[fixture.source.m_iReflectionBuffer]->bNewSlot;
#if UINTPTR_MAX > UINT32_MAX
            Check(newSlot, "released polygon slot is assigned as a new public identity");
#else
            Check(!newSlot, "original Win32 public tag still matches the same DWORD polygon key");
#endif
        }
    }
    DetachBorrowed(face);
}

int main(int argc, char** argv) {
    Check(argc == 2, "one case argument");
    Run(argv[1]);
    std::puts("PASS exact production geometry/command/reflection component");
}
