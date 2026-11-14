// Independent full-run expansion oracle compared with actual loader and visibility consumers.
static std::vector<byte> Oracle(const byte* input, std::size_t bytes, std::size_t rowBytes) {
    if (!input) {
        return std::vector<byte>(rowBytes, 255);
    }
    std::vector<byte> result;
    std::size_t cursor = 0;
    while (result.size() < rowBytes) {
        assert(cursor < bytes);
        const byte value = input[cursor++];
        if (value) {
            result.push_back(value);
        } else {
            assert(cursor < bytes && input[cursor] > 0);
            result.insert(result.end(), input[cursor++], 0);
        }
    }
    result.resize(rowBytes);
    return result;
}

static void RunCorpus(const std::filesystem::path& assets) {
    std::size_t maps = 0;
    std::size_t rows = 0;
    std::size_t cacheMaps = 0;
    bool largePhysical = false;
    constexpr int DiskMarksurfaceIndices = 65536;
    std::vector<msurface_t*> marksurfaces(DiskMarksurfaceIndices);
    for (const auto& item : std::filesystem::recursive_directory_iterator(assets)) {
        if (!item.is_regular_file() || item.path().extension() != ".bsp") {
            continue;
        }
        std::ifstream stream(item.path(), std::ios::binary);
        std::vector<byte> disk((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        assert(disk.size() <= INT_MAX);
        assert(ModelVisibility::BrushLumps(disk.data(), static_cast<int>(disk.size())));
        dheader_t header;
        std::memcpy(&header, disk.data(), sizeof(header));
        assert(header.version == BSPVERSION || header.version == Q1BSP_VERSION);
        model_t* model = LoadFixture();
        loadmodel = model;
        model->marksurfaces = marksurfaces.data();
        mod_base = disk.data();
        Mod_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
        Mod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
        dmodel_t world;
        std::memcpy(&world, disk.data() + header.lumps[LUMP_MODELS].fileofs, sizeof(world));
        model->numleafs = world.visleafs;
        const int physical = header.lumps[LUMP_LEAFS].filelen / static_cast<int>(sizeof(dleaf_t));
        const std::size_t logicalBytes = (static_cast<std::size_t>(world.visleafs) + 7) / 8;
        largePhysical |= physical > MAX_MAP_LEAFS;
        std::vector<std::vector<byte>> expected;
        constexpr int MaximumCacheCorpusLeaves = 256;
        for (int i = 0; i < physical; ++i) {
            dleaf_t leaf;
            std::memcpy(&leaf, disk.data() + header.lumps[LUMP_LEAFS].fileofs + i * sizeof(leaf), sizeof(leaf));
            const byte* input = leaf.visofs == -1 ? nullptr :
                disk.data() + header.lumps[LUMP_VISIBILITY].fileofs + leaf.visofs;
            const std::size_t sourceBytes = input ? header.lumps[LUMP_VISIBILITY].filelen - leaf.visofs : 0;
            auto want = Oracle(input, sourceBytes, logicalBytes);
            assert(std::memcmp(Mod_DecompressVis(model->leafs[i].compressed_vis, model),
                               want.data(), logicalBytes) == 0);
            if (i <= world.visleafs && world.visleafs <= MaximumCacheCorpusLeaves) {
                expected.push_back(want);
            }
            ++rows;
        }
        if (world.visleafs > 0 && world.visleafs <= MaximumCacheCorpusLeaves) {
            CM_CalcPAS(model);
            for (int i = 0; i <= world.visleafs; ++i) {
                std::vector<byte> audible = expected[i];
                for (int leaf = 1; leaf <= world.visleafs; ++leaf) {
                    if (expected[i][(leaf - 1) / 8] & (1 << ((leaf - 1) % 8))) {
                        for (std::size_t byteIndex = 0; byteIndex < logicalBytes; ++byteIndex) {
                            audible[byteIndex] |= expected[leaf][byteIndex];
                        }
                    }
                }
                assert(std::memcmp(CM_LeafPVS(i), expected[i].data(), logicalBytes) == 0);
                assert(std::memcmp(CM_LeafPAS(i), audible.data(), logicalBytes) == 0);
            }
            ++cacheMaps;
        }
        ++maps;
    }
    assert(maps > 0 && rows > 0 && cacheMaps > 0);
    std::printf("PASS corpus: %zu maps, %zu physical rows, %zu PVS/PAS maps, physical >8192: %s\n",
                maps, rows, cacheMaps, largePhysical ? "yes" : "no");
    CM_FreePAS();
}
