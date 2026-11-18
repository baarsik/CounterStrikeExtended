#include "fixture.h"
#include <fstream>
#include <iterator>
#ifndef ARCHIVE_OLD
#include "save_archive_impl.h"
#endif
#include "owner.h"

#ifdef ARCHIVE_OLD
int main(int argc, char **argv) {
    try {
        check(argc == 2, "mode required");
        reset();
        if (std::string(argv[1]) == "reader") {
            MemoryFile input{archive(260)};
            DirectoryExtract(&input, 3);
            check(files["SAVE/first.HL1"].data == Bytes({0x11, 0x22, 0x33})
                && files["SAVE/last.HL3"].data == Bytes({0xfe, 0, 0xed}),
                "old host-width reader cannot decode fixed260 multi-entry archive");
        } else {
            addName("first.HL1", std::string(argv[1]) == "short" ? 0 : MAX_PATH);
            files["SAVE/first.HL1"].data = {0x11, 0x22, 0x33};
            MemoryFile output;
            DirectoryCopy("SAVE/*.HL?", &output);
            check(output.data == entry(260, "first.HL1", {0x11, 0x22, 0x33}),
                "old actual DirectoryCopy emits host filename width instead of fixed260");
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
#ifndef ARCHIVE_OLD
using cse::save_archive::Layout;
using cse::save_archive::ReadLayout;
using cse::save_archive::Extract;
using cse::save_archive::WriteDirectory;

void assertPayloads() {
    check(files.at("SAVE/first.HL1").data == Bytes({0x11, 0x22, 0x33}), "first payload changed");
    check(files.at("SAVE/empty.HL2").data.empty(), "empty payload changed");
    check(files.at("SAVE/last.HL3").data == Bytes({0xfe, 0, 0xed}), "last payload changed");
}

void validReaders() {
    for (auto width : {260, 1024}) {
        reset();
        MemoryFile input{archive(width, 0xa5)};
        const auto original = input.data;
        Layout layout{};
        check(ReadLayout(&input, 3, layout), "valid layout rejected");
        check(input.position == 0 && layout.nameBytes == static_cast<unsigned int>(width)
            && layout.recordsEnd == original.size(), "layout/cursor wrong");
        check(Extract(&input, layout), "valid extraction failed");
        assertPayloads();
        check(input.position == input.data.size() && input.data == original, "input bytes/cursor changed");
        reset();
        input = {original};
        check(DirectoryExtract(&input, 3), "actual DirectoryExtract failed");
        assertPayloads();
        reset();
        files["SAVE/slot.sav"].data = original;
        declaredCount = 3;
        mapped = 0;
        check(Host_Load("slot") == 1 && mapped == 1, "actual Host_Load failed");
        assertPayloads();
        check(files.at("SAVE/slot.sav").data == original, "Host_Load modified input");
    }
    for (auto width : {260, 1024}) {
        reset();
        const auto longName = std::string(width - 5, 'a') + ".HL1";
        MemoryFile input{entry(width, longName, {0x71})};
        Layout layout{};
        check(ReadLayout(&input, 1, layout) && Extract(&input, layout), "long terminated old name rejected");
        check(files.at("SAVE/" + longName).data == Bytes({0x71}), "long name truncated");
    }
    for (const char *name : {"map.v2.HL1", "map name.HL2", "map+name.HL3", "map\xc3\xb3.HL1"}) {
        reset();
        MemoryFile input{entry(260, name, {0x71})};
        Layout layout{};
        check(ReadLayout(&input, 1, layout) && Extract(&input, layout), "safe existing map-state name rejected");
        check(files.at(std::string("SAVE/") + name).data == Bytes({0x71}), "safe filename changed");
    }
    reset();
    MemoryFile empty;
    Layout layout{1, 2, 3, 4, 5};
    check(ReadLayout(&empty, 0, layout) && layout.nameBytes == 260 && layout.recordsEnd == 0,
        "empty archive rejected");
    check(Extract(&empty, layout) && outputOpens == 0, "empty archive created output");
    MemoryFile prefixed{{0x71, 0x72}};
    auto data = archive(260);
    prefixed.data.insert(prefixed.data.end(), data.begin(), data.end());
    prefixed.position = 2;
    check(ReadLayout(&prefixed, 3, layout) && layout.start == 2 && prefixed.position == 2, "nonzero start failed");
    check(Extract(&prefixed, layout), "nonzero start extraction failed");
}

void rejected(const Bytes &data, int count) {
    reset();
    MemoryFile input{data};
    Layout layout{1, 2, 3, 4, 5};
    check(!ReadLayout(&input, count, layout), "malformed layout accepted");
    check(layout.start == 0 && layout.end == 0 && layout.fileCount == 0 && layout.nameBytes == 0
        && layout.recordsEnd == 0,
        "failed layout not cleared");
    input.position = 0;
    check(!Extract(&input, {0, static_cast<unsigned int>(data.size()), count, 260,
        static_cast<unsigned int>(data.size())}), "forged layout accepted");
    check(input.data == data && outputOpens == 0, "validation wrote outputs/input");
    input.position = 0;
    check(!DirectoryExtract(&input, count), "DirectoryExtract accepted malformed input");
    check(outputOpens == 0, "DirectoryExtract opened malformed outputs");
    files["SAVE/bad.sav"].data = data;
    declaredCount = count;
    mapped = 0;
    check(Host_Load("bad") == 0 && mapped == 0 && outputOpens == 0, "Host_Load accepted malformed input");
    check(files.at("SAVE/bad.sav").data == data, "Host_Load modified malformed input");
}

// DOS device names must fail complete archive validation before the valid first output opens.
void reservedDeviceNames() {
    bool allSafe = true;
    const auto expect = [&](bool value, const char *message) {
        if (!value) {
            allSafe = false;
            std::cerr << message << '\n';
        }
    };
    const char *reservedNames[] = {
        "NUL.HL1", "con.HL2", "pRn.HL3", "AuX.HL1", "CONIN$.HL1", "conout$.HL2",
        "COM1.HL2", "com9.HL1", "LPT1.HL3", "lPt9.HL2", "NUL.extra.HL1",
        "NUL .HL1", " NUL.HL1", "  nUl  .extra.HL2",
        "COM\xb9.HL1", "lpt\xb2.HL2", "COM\xb3.HL3",
        "COM\xc2\xb9.HL1", "lpt\xc2\xb2.HL2", "COM\xc2\xb3.HL3"
    };
    for (const char *name : reservedNames) {
        for (auto width : {260, 1024}) {
            const auto first = entry(width, "first.HL1", {0x11, 0x22, 0x33});
            const auto second = entry(width, name, {0xfe, 0, 0xed}, 0xa5);
            Bytes data = first;
            data.insert(data.end(), second.begin(), second.end());
            reset();
            MemoryFile input{data};
            Layout layout{1, 2, 3, 4, 5};
            const bool accepted = ReadLayout(&input, 2, layout);
            expect(!accepted && layout.start == 0 && layout.end == 0 && layout.fileCount == 0
                && layout.nameBytes == 0 && layout.recordsEnd == 0, "reserved later entry accepted by ReadLayout");
            expect(input.data == data && outputOpens == 0, "ReadLayout changed input/opened outputs");
            reset();
            input = {data};
            expect(!Extract(&input, {0, static_cast<unsigned int>(data.size()), 2,
                static_cast<unsigned int>(width), static_cast<unsigned int>(data.size())}),
                "reserved later entry accepted by Extract");
            expect(input.data == data && outputOpens == 0, "reserved Extract opened outputs/changed input");
            reset();
            input = {data};
            expect(!DirectoryExtract(&input, 2), "reserved later entry accepted by DirectoryExtract");
            expect(input.data == data && outputOpens == 0, "reserved DirectoryExtract opened outputs/changed input");
            reset();
            files["SAVE/reserved.sav"].data = data;
            declaredCount = 2;
            mapped = 0;
            expect(Host_Load("reserved") == 0 && mapped == 0 && outputOpens == 0,
                "reserved later entry accepted/opened outputs through Host_Load");
            expect(files.at("SAVE/reserved.sav").data == data, "reserved Host_Load input changed");
        }
        reset();
        addName(name);
        files[std::string("SAVE/") + name].data = {0x71};
        MemoryFile output;
        expect(!WriteDirectory("SAVE/*.HL?", &output, 1) && output.data.empty(), "reserved writer name accepted");
        expect(!DirectoryCopy("SAVE/*.HL?", &output, 1) && output.data.empty(), "reserved DirectoryCopy accepted");
        check(files.at(std::string("SAVE/") + name).data == Bytes({0x71}), "reserved writer changed input");
    }
    const char *safeNames[] = {
        "NULmap.HL1", "CONtext.HL2", "AUXiliary.HL3", "COM10.HL1", "COM1map.HL2",
        "LPT10.HL3", "LPT9map.HL1", "map.NUL.HL2", "COM\xc2\xb9map.HL3", "map\xc3\xb3.HL1"
    };
    for (const char *name : safeNames) {
        for (auto width : {260, 1024}) {
            reset();
            const auto data = entry(width, name, {0x71});
            MemoryFile input{data};
            Layout layout{};
            check(ReadLayout(&input, 1, layout) && Extract(&input, layout), "safe device lookalike rejected");
            check(files.at(std::string("SAVE/") + name).data == Bytes({0x71}) && input.data == data,
                "safe device lookalike output/input changed");
        }
        reset();
        addName(name);
        files[std::string("SAVE/") + name].data = {0x71};
        MemoryFile output;
        check(WriteDirectory("SAVE/*.HL?", &output, 1) && output.data == entry(260, name, {0x71}),
            "safe device lookalike writer rejected/changed bytes");
    }
    check(allSafe, "reserved device rejection matrix failed");
}

void malformedReaders() {
    rejected(archive(260), -1);
    rejected(archive(260), INT_MAX);
    rejected(archive(260), 0);
    rejected({}, 1);
    rejected({1}, 0);
    auto data = archive(260);
    data.push_back(0);
    rejected(data, 3);
    for (std::size_t length : {0u, 1u, 259u, 260u, 263u, 264u, 266u, 530u, 797u}) {
        data = archive(260);
        data.resize(length);
        rejected(data, 3);
    }
    data = entry(260, "first.HL1", {});
    data[260] = 0xff;
    data[261] = 0xff;
    data[262] = 0xff;
    data[263] = 0xff;
    rejected(data, 1);
    data[263] = 0x7f;
    rejected(data, 1);
    data = entry(260, "first.HL1", {});
    std::fill(data.begin(), data.begin() + 260, 'a');
    rejected(data, 1);
    data = entry(1024, "first.HL1", {});
    std::fill(data.begin(), data.begin() + 1024, 'a');
    rejected(data, 1);
    for (const char *name : {"../a.HL1", "/a.HL1", "a/b.HL1", "a\\b.HL1", "C:a.HL1", "~a.HL1",
        "a..b.HL1", "a.sav", "a.HL", "a.HL12", "a.HL?", ".HL1", "a.HL1:bad"}) {
        rejected(entry(260, name, {}), 1);
    }
    for (auto width : {260, 1024}) {
        data = archive(width);
        // The first payload is three bytes. A bad second name must prevent even the first output.
        std::memcpy(data.data() + width + 7, "../bad.HL2", 11);
        rejected(data, 3);
    }
    // Both full layouts end at 2056: legacy lengths764,764; native lengths0,0.
    data.assign(2056, 0);
    std::memcpy(data.data(), "first.HL1", 10);
    data[260] = 0xfc;
    data[261] = 2;
    std::memcpy(data.data() + 1028, "last.HL3", 9);
    data[1288] = 0xfc;
    data[1289] = 2;
    rejected(data, 2);
    reset();
    MemoryFile input{archive(1024)};
    Layout layout{};
    check(ReadLayout(&input, 3, layout), "revalidation setup failed");
    ++layout.end;
    check(!Extract(&input, layout) && outputOpens == 0, "changed extent accepted");
    --layout.end;
    layout.nameBytes = 260;
    check(!Extract(&input, layout) && outputOpens == 0, "changed width accepted");
    reset();
    input = {archive(260)};
    input.position = 1;
    failSeek = true;
    check(!ReadLayout(&input, 3, layout) && outputOpens == 0, "failed seek accepted");
    reset();
    input = {archive(260)};
    readBudget = 259;
    check(!ReadLayout(&input, 3, layout) && outputOpens == 0, "short read accepted");
    reset();
    input = {archive(260)};
    wrongReadPosition = true;
    check(!ReadLayout(&input, 3, layout) && outputOpens == 0, "wrong read cursor accepted");
    reset();
    input = {archive(260)};
    fakeSize = static_cast<unsigned int>(INT_MAX) + 1u;
    check(!ReadLayout(&input, 3, layout), "unsigned offset narrowed into signed seek");
}

void writerSetup() {
    reset();
    addName("first.HL1");
    addName("empty.HL2");
    addName("last.HL3");
    files["SAVE/first.HL1"].data = {0x11, 0x22, 0x33};
    files["SAVE/empty.HL2"].data = {};
    files["SAVE/last.HL3"].data = {0xfe, 0, 0xed};
    declaredCount = 3;
}

void writers() {
    writerSetup();
    MemoryFile output;
    check(WriteDirectory("SAVE/*.HL?", &output, 3), "writer failed");
    check(output.data == archive(260), "writer bytes differ from canonical260 independent fixture");
    writerSetup();
    output = {};
    check(DirectoryCopy("SAVE/*.HL?", &output, 3) && output.data == archive(260), "DirectoryCopy failed");
    writerSetup();
    check(SaveGameSlot("slot", "comment") == 1, "SaveGameSlot success failed");
    const auto &saved = files.at("SAVE/slot.sav").data;
    check(saved.size() == 20 + archive(260).size(), "SaveGameSlot envelope size changed");
    check(Bytes(saved.begin(), saved.begin() + 8) == Bytes({0x4a, 0x53, 0x41, 0x56, 0x71, 0, 0, 0}),
        "JSAV version changed");
    check(Bytes(saved.begin() + 20, saved.end()) == archive(260), "SaveGameSlot canonical bytes wrong");
    for (int count : {-1, 0, 2, 4}) {
        writerSetup();
        output = {};
        check(!WriteDirectory("SAVE/*.HL?", &output, count), "wrong count accepted");
    }
    reset();
    output = {};
    check(WriteDirectory("SAVE/*.HL?", &output, 0) && output.data.empty(), "empty writer failed");
    for (auto name : {std::string(255, 'a') + ".HL1", std::string(256, 'a') + ".HL1"}) {
        reset();
        addName(name);
        files["SAVE/" + name] = {};
        output = {};
        check(WriteDirectory("SAVE/*.HL?", &output, 1) == (name.size() == 259), "writer name bound wrong");
    }
    reset();
    auto unterminated = std::make_unique<char[]>(260);
    std::memset(unterminated.get(), 'a', 260);
    findNames.push_back(std::move(unterminated));
    output = {};
    check(!WriteDirectory("SAVE/*.HL?", &output, 1) && output.data.empty(), "unterminated name accepted");
    reset();
    addName("../a.HL1");
    check(!WriteDirectory("SAVE/*.HL?", &output, 1), "unsafe writer name accepted");
}

void assertSourceBytes() {
    check(files.at("SAVE/first.HL1").data == Bytes({0x11, 0x22, 0x33})
        && files.at("SAVE/empty.HL2").data.empty()
        && files.at("SAVE/last.HL3").data == Bytes({0xfe, 0, 0xed}), "writer modified source bytes");
}

void mixedCaseMapStateExtensions() {
    for (const char *name : {"c1a0.hl1", "c1a0.Hl2", "c1a0.hL3"}) {
        reset();
        addName(name);
        const auto path = std::string("SAVE/") + name;
        const Bytes payload = {0x11, 0x22};
        files[path].data = payload;
        MemoryFile output;
        check(WriteDirectory("SAVE/*.HL?", &output, 1), "enumerated mixed-case map state failed to save");
        check(output.data == entry(260, name, payload), "writer changed the mixed-case record name");

        for (const auto width : {260, 1024}) {
            reset();
            MemoryFile input{entry(width, name, payload)};
            check(DirectoryExtract(&input, 1), "mixed-case map state failed to load");
            check(files.at(path).data == payload, "extraction changed the map state name or payload");
        }
    }
}

void chunkedCopy() {
    const Bytes payload(8201, 0xd3);
    for (auto width : {260, 1024}) {
        reset();
        MemoryFile input{entry(width, "chunked.HL1", payload)};
        const auto original = input.data;
        Layout layout{};
        check(ReadLayout(&input, 1, layout) && Extract(&input, layout), "chunked extraction failed");
        check(files.at("SAVE/chunked.HL1").data == payload && input.data == original, "chunked payload changed");
    }
    reset();
    addName("chunked.HL1");
    files["SAVE/chunked.HL1"].data = payload;
    MemoryFile output;
    check(WriteDirectory("SAVE/*.HL?", &output, 1), "chunked writer failed");
    check(output.data == entry(260, "chunked.HL1", payload), "chunked canonical bytes wrong");
    output = {};
    readBudget = 4097;
    check(!WriteDirectory("SAVE/*.HL?", &output, 1), "chunked partial read accepted");
    check(files.at("SAVE/chunked.HL1").data == payload, "chunked writer modified input");
}

void faults() {
    for (int budget : {0, 259, 260, 263, 264, 265, 530, 797}) {
        writerSetup();
        MemoryFile output;
        writeBudget = budget;
        check(!WriteDirectory("SAVE/*.HL?", &output, 3), "short writer success");
        assertSourceBytes();
        writerSetup();
        writeBudget = budget + 20;
        check(SaveGameSlot("slot", "comment") == 0, "SaveGameSlot swallowed archive copy/write failure");
        assertSourceBytes();
    }
    for (int budget : {0, 1, 19}) {
        writerSetup();
        writeBudget = budget;
        check(SaveGameSlot("slot", "comment") == 0, "SaveGameSlot swallowed envelope write failure");
        assertSourceBytes();
    }
    writerSetup();
    failOpen = true;
    MemoryFile output;
    check(!WriteDirectory("SAVE/*.HL?", &output, 3), "open failure success");
    check(SaveGameSlot("slot", "comment") == 0, "SaveGameSlot swallowed output open failure");
    writerSetup();
    files.erase("SAVE/last.HL3");
    check(SaveGameSlot("slot", "comment") == 0, "SaveGameSlot swallowed input open failure");
    writerSetup();
    readBudget = 2;
    check(SaveGameSlot("slot", "comment") == 0, "SaveGameSlot swallowed payload read failure");
    writerSetup();
    wrongWritePosition = true;
    output = {};
    check(!WriteDirectory("SAVE/*.HL?", &output, 3), "wrong write cursor accepted");
    writerSetup();
    fakeSize = static_cast<unsigned int>(INT_MAX) + 1u;
    check(!WriteDirectory("SAVE/*.HL?", &output, 3), "oversized input accepted");
    for (int fault = 0; fault < 3; ++fault) {
        reset();
        MemoryFile input{archive(260)};
        const auto original = input.data;
        Layout layout{};
        check(ReadLayout(&input, 3, layout), "fault setup failed");
        if (fault == 0) {
            failOpen = true;
        } else if (fault == 1) {
            // The successful260 validation reads three headers; then extraction reads its first header.
            readBudget = 3 * 264 + 264 + 2;
        } else {
            writeBudget = 2;
        }
        check(!Extract(&input, layout), "extract copy fault accepted");
        check(input.data == original, "extract fault modified input");
    }
}

Bytes repeated(const Bytes &block, std::size_t copies) {
    Bytes bytes;
    for (std::size_t index = 0; index < copies; ++index) {
        bytes.insert(bytes.end(), block.begin(), block.end());
    }
    return bytes;
}

void repeatReader(int width, int copies) {
    reset();
    const auto block = archive(width, 0xa5);
    const auto bytes = repeated(block, copies);
    MemoryFile input{bytes};
    Layout layout{};
    check(ReadLayout(&input, 3, layout), "complete identical repeated sets rejected");
    check(layout.start == 0 && layout.end == bytes.size() && layout.recordsEnd == block.size()
        && layout.nameBytes == static_cast<unsigned int>(width) && input.position == 0,
        "repeated layout extent/width/cursor wrong");
    check(Extract(&input, layout), "repeated extraction failed");
    assertPayloads();
    check(outputOpens == 3 && input.position == bytes.size() && input.data == bytes,
        "repeated extraction duplicated outputs or changed input/cursor");
    reset();
    input = {bytes};
    check(DirectoryExtract(&input, 3) && outputOpens == 3, "DirectoryExtract repeated sets failed");
    assertPayloads();
    reset();
    files["SAVE/slot.sav"].data = bytes;
    declaredCount = 3;
    mapped = 0;
    check(Host_Load("slot") == 1 && mapped == 1 && outputOpens == 3, "Host_Load repeated sets failed");
    assertPayloads();
    check(files.at("SAVE/slot.sav").data == bytes && files.at("SAVE/slot.sav").closed,
        "repeat Host_Load changed input or leaked handle");
    reset();
    input = {{0x71, 0x72}};
    input.data.insert(input.data.end(), bytes.begin(), bytes.end());
    input.position = 2;
    check(ReadLayout(&input, 3, layout) && layout.start == 2 && layout.recordsEnd == 2 + block.size()
        && input.position == 2,
        "prefixed repeated layout failed");
    check(Extract(&input, layout) && outputOpens == 3 && input.position == input.data.size(),
        "prefixed repeated extraction failed");
}

void repeatRejections() {
    for (int width : {260, 1024}) {
        const auto block = archive(width, 0xa5);
        // Name, unused name padding, length and payload must all be byte-identical.
        const std::size_t changes[] = {0, 20, static_cast<std::size_t>(width),
            static_cast<std::size_t>(width + 4), block.size() - 1};
        for (auto offset : changes) {
            auto bytes = repeated(block, 3);
            bytes[2 * block.size() + offset] ^= 1;
            rejected(bytes, 3);
        }
        for (auto tailBytes : {std::size_t{1}, static_cast<std::size_t>(width), block.size() - 1}) {
            auto bytes = repeated(block, 2);
            bytes.insert(bytes.end(), block.begin(), block.begin() + tailBytes);
            rejected(bytes, 3);
        }
        auto bytes = repeated(block, 2);
        bytes.push_back(0);
        rejected(bytes, 3);
        bytes = block;
        const auto extra = entry(width, "other.HL1", {0x71});
        bytes.insert(bytes.end(), extra.begin(), extra.end());
        rejected(bytes, 3);
        rejected(repeated(block, 3), 0);
        for (const char *name : {"../bad.HL1", "NUL.HL2", "a/b.HL3"}) {
            auto unsafe = entry(width, "first.HL1", {0x71});
            const auto bad = entry(width, name, {});
            unsafe.insert(unsafe.end(), bad.begin(), bad.end());
            rejected(repeated(unsafe, 3), 2);
        }
    }
    // Independently placed fields: 260 repeat / 1024 exact, both repeat, and the reverse.
    for (int variant = 0; variant < 3; ++variant) {
        Bytes block(1028, 0);
        std::memcpy(block.data(), "first.HL1", 10);
        if (variant == 2) {
            block[260] = 0x00; // LE32(1792): 260 record ends at2056; 1024 ends at1028.
            block[261] = 0x07;
        } else {
            block[260] = 0xfc; // LE32(764): 260 record ends at1028.
            block[261] = 0x02;
            block[1024] = 0x04; // LE32(1028): 1024 record ends at2056.
            block[1025] = 0x04;
        }
        rejected(repeated(block, variant == 1 ? 4 : 2), 1);
    }
}

void repeatStaleLayouts() {
    const auto bytes = repeated(archive(260), 3);
    for (int mutation = 0; mutation < 12; ++mutation) {
        reset();
        MemoryFile input{bytes};
        Layout layout{};
        check(ReadLayout(&input, 3, layout), "repeat stale-layout setup failed");
        if (mutation == 0) {
            ++layout.start;
        } else if (mutation == 1) {
            --layout.end;
        } else if (mutation == 2) {
            ++layout.fileCount;
        } else if (mutation == 3) {
            input.data.pop_back();
        } else if (mutation == 4) {
            input.data.push_back(0);
        } else if (mutation == 5) {
            input.data.back() ^= 1;
        } else if (mutation == 6) {
            layout.recordsEnd = layout.end;
        } else if (mutation == 7) {
            layout.recordsEnd = layout.end + 1;
        } else if (mutation == 8) {
            --layout.recordsEnd;
        } else if (mutation == 9) {
            layout.fileCount *= 3; // A canonical interpretation must not reuse the old first-set boundary.
        } else if (mutation == 10) {
            const auto block = archive(260);
            input.data.insert(input.data.end(), block.begin(), block.end());
        } else {
            layout.start = 2;
            layout.recordsEnd = 1;
        }
        const auto changed = input.data;
        check(!Extract(&input, layout) && outputOpens == 0 && input.data == changed,
            "stale repeat layout accepted or opened output");
    }
}

void assertClosedOutputs() {
    for (const auto &item : files) {
        check(item.second.closed, "extraction IO failure leaked an output handle");
    }
}

void repeatFaults(int width) {
    constexpr std::size_t LargePayloadBytes = 8201;
    const auto bytes = repeated(entry(width, "chunked.HL1", Bytes(LargePayloadBytes, 0xd3)), 3);
    reset();
    MemoryFile probe{bytes};
    Layout layout{};
    check(ReadLayout(&probe, 1, layout), "repeat fault setup failed");
    const auto validationReads = readObservations;
    const auto validationSeeks = seekObservations;
    for (std::size_t call = 0; call < validationReads.size(); ++call) {
        for (bool wrongCursor : {false, true}) {
            reset();
            MemoryFile input{bytes};
            if (wrongCursor) {
                wrongReadCall = static_cast<int>(call);
            } else {
                failReadCall = static_cast<int>(call);
            }
            check(!ReadLayout(&input, 1, layout) && outputOpens == 0 && input.position == 0,
                "repeat validation read/cursor fault accepted or failed restoration");
            check(layout.start == 0 && layout.end == 0 && layout.fileCount == 0 && layout.nameBytes == 0
                && layout.recordsEnd == 0,
                "repeat validation failure retained layout");
        }
    }
    for (std::size_t call = 0; call < validationSeeks.size(); ++call) {
        if (validationSeeks[call].before == validationSeeks[call].target) {
            continue; // A no-op seek has no observable failure when already at its target.
        }
        reset();
        MemoryFile input{bytes};
        failSeekCall = static_cast<int>(call);
        check(!ReadLayout(&input, 1, layout) && outputOpens == 0, "repeat validation seek fault accepted");
        if (call + 1 != validationSeeks.size()) {
            check(input.position == 0, "repeat validation seek fault failed later restoration");
        }
    }
    reset();
    probe = {bytes};
    check(ReadLayout(&probe, 1, layout), "repeat extraction fault setup failed");
    readObservations.clear();
    seekObservations.clear();
    check(Extract(&probe, layout), "repeat extraction fault probe failed");
    const auto extractionReads = readObservations;
    const auto extractionSeeks = seekObservations;
    for (std::size_t call = 0; call < extractionReads.size(); ++call) {
        reset();
        MemoryFile input{bytes};
        failReadCall = static_cast<int>(call);
        check(!Extract(&input, layout) && input.data == bytes, "repeat extraction read fault accepted");
        check(outputOpens == extractionReads[call].outputs, "repeat preflight read failure opened output");
        assertClosedOutputs();
    }
    for (std::size_t call = 0; call < extractionSeeks.size(); ++call) {
        if (extractionSeeks[call].before == extractionSeeks[call].target) {
            continue;
        }
        reset();
        MemoryFile input{bytes};
        failSeekCall = static_cast<int>(call);
        check(!Extract(&input, layout) && input.data == bytes, "repeat extraction seek fault accepted");
        check(outputOpens == extractionSeeks[call].outputs, "repeat preflight seek failure opened output");
        assertClosedOutputs();
        if (call + 1 == extractionSeeks.size()) {
            check(outputOpens == 1 && input.position == bytes.size() / 3,
                "final EOF-seek failure contract changed");
            check(files.at("SAVE/chunked.HL1").data == Bytes(LargePayloadBytes, 0xd3),
                "final EOF-seek failure removed already copied output");
        }
    }
    for (bool openFailure : {false, true}) {
        reset();
        MemoryFile input{bytes};
        failOpen = openFailure;
        writeBudget = openFailure ? -1 : 2;
        check(!Extract(&input, layout) && input.data == bytes, "repeat output fault accepted");
        check(outputOpens == (openFailure ? 0 : 1), "repeat output failure opened unexpected files");
        if (!openFailure) {
            check(files.at("SAVE/chunked.HL1").data == Bytes(2, 0xd3), "partial output contract changed");
        }
        assertClosedOutputs();
    }
}

void repeatResources() {
    for (int width : {260, 1024}) {
        for (std::size_t payloadBytes : {std::size_t{0}, std::size_t{8201}}) {
            reset();
            const auto name = std::string(width - 5, 'a') + ".HL1";
            const auto bytes = repeated(entry(width, name, Bytes(payloadBytes, 0xd3), 0xa5), 31);
            MemoryFile input{bytes};
            Layout layout{};
            check(ReadLayout(&input, 1, layout) && Extract(&input, layout), "many repeated sets failed");
            check(outputOpens == 1 && files.at("SAVE/" + name).data == Bytes(payloadBytes, 0xd3),
                "many repeated sets changed/duplicated output");
            std::uint64_t readBytes = 0;
            for (const auto &read : readObservations) {
                check(read.requested <= 4096, "repeat validation allocated an unbounded read");
                readBytes += read.actual;
            }
            // Two candidate widths, two validations, and one extraction must remain linear in physical bytes.
            check(readBytes <= 8 * bytes.size() + 4096, "repeat validation rereads input superlinearly");
        }
    }
}

void corpusReader(const char *path, unsigned int start, int count, unsigned int firstEnd, unsigned int end) {
    std::ifstream stream(path, std::ios::binary);
    check(stream.is_open(), "cannot read explicit corpus path");
    const Bytes bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    check(!stream.bad(), "corpus stream failed");
    check(bytes.size() == end && start < firstEnd && firstEnd <= end && count > 0, "corpus manifest mismatch");
    // Independently decode first-set fields, without production ReadRecord/ReadLayout helpers.
    std::map<std::string, Bytes> expected;
    std::size_t cursor = start;
    for (int index = 0; index < count; ++index) {
        check(cursor <= firstEnd && firstEnd - cursor >= 264, "corpus first-set header outside manifest");
        const auto nul = std::find(bytes.begin() + cursor, bytes.begin() + cursor + 260, 0);
        check(nul != bytes.begin() + cursor + 260, "corpus expected name lacks terminator");
        const std::string name(bytes.begin() + cursor, nul);
        const auto length = static_cast<std::uint32_t>(bytes[cursor + 260])
            | (static_cast<std::uint32_t>(bytes[cursor + 261]) << 8)
            | (static_cast<std::uint32_t>(bytes[cursor + 262]) << 16)
            | (static_cast<std::uint32_t>(bytes[cursor + 263]) << 24);
        cursor += 264;
        check(length <= firstEnd - cursor, "corpus expected payload outside first set");
        check(expected.emplace("SAVE/" + name, Bytes(bytes.begin() + cursor, bytes.begin() + cursor + length)).second,
            "corpus first-set filenames are not unique");
        cursor += length;
    }
    check(cursor == firstEnd, "corpus first-set endpoint disagrees with manifest");
    reset();
    MemoryFile input{bytes, start};
    Layout layout{};
    check(ReadLayout(&input, count, layout), "production reader rejected independently verified corpus");
    check(layout.start == start && layout.end == end && layout.recordsEnd == firstEnd
        && layout.nameBytes == 260 && input.position == start,
        "corpus production layout/cursor differs from manifest");
    check(Extract(&input, layout) && outputOpens == count && input.position == end && input.data == bytes,
        "corpus production extraction changed input/count/cursor");
    for (const auto &item : expected) {
        check(files.at(item.first).data == item.second && files.at(item.first).closed,
            "corpus production extraction payload/handle mismatch");
    }
    std::cout << "PASS actual corpus production reader and in-memory Extract: " << path
              << " start=" << start << " firstEnd=" << firstEnd << " physicalEnd=" << end
              << " records=" << count << "; no full-game/envelope/entity restoration claim\n";
}

int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--repeat") {
            repeatReader(std::stoi(argv[2]), std::stoi(argv[3]));
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--corpus") {
            corpusReader(argv[2], static_cast<unsigned int>(std::stoul(argv[3])), std::stoi(argv[4]),
                static_cast<unsigned int>(std::stoul(argv[5])), static_cast<unsigned int>(std::stoul(argv[6])));
            return 0;
        }
        check(argc == 1, "unexpected fixture arguments");
        validReaders();
        reservedDeviceNames();
        malformedReaders();
        writers();
        mixedCaseMapStateExtensions();
        chunkedCopy();
        faults();
        repeatRejections();
        for (int width : {260, 1024}) {
            for (int copies : {2, 3, 7}) {
                repeatReader(width, copies);
            }
        }
        repeatStaleLayouts();
        for (int width : {260, 1024}) {
            repeatFaults(width);
        }
        repeatResources();
        std::cout << "canonical writer, compatible readers, actual owner integrations and fault matrix passed; host="
            << MAX_PATH << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
