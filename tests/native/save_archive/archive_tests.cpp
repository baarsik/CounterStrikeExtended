#include "fixture.h"
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
        check(input.position == 0 && layout.nameBytes == static_cast<unsigned int>(width), "layout/cursor wrong");
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
    Layout layout{1, 2, 3, 4};
    check(ReadLayout(&empty, 0, layout) && layout.nameBytes == 260, "empty archive rejected");
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
    Layout layout{1, 2, 3, 4};
    check(!ReadLayout(&input, count, layout), "malformed layout accepted");
    check(layout.start == 0 && layout.end == 0 && layout.fileCount == 0 && layout.nameBytes == 0,
        "failed layout not cleared");
    input.position = 0;
    check(!Extract(&input, {0, static_cast<unsigned int>(data.size()), count, 260}), "forged layout accepted");
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

int main() {
    try {
        validReaders();
        malformedReaders();
        writers();
        chunkedCopy();
        faults();
        std::cout << "canonical writer, compatible readers, actual owner integrations and fault matrix passed; host="
            << MAX_PATH << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
