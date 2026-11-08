#include "fixture.h"

namespace {
void PutInteger(std::vector<unsigned char> &bytes, std::size_t index, std::int32_t value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    constexpr unsigned int ByteBits = 8;
    for (unsigned int byte = 0; byte < sizeof(bits); ++byte) {
        bytes[index * sizeof(bits) + byte] = static_cast<unsigned char>(bits >> (byte * ByteBits));
    }
}

std::vector<unsigned char> MakeSave() {
    // Twelve token bytes align the legacy Win32 table but misalign a native 8-byte table.
    const unsigned char tokens[] = {'a', 'l', 'p', 'h', 'a', 0, 0, 'b', 'e', 't', 'a', 0};
    const unsigned char data[] = {0x91, 0, 0x5a};
    std::vector<unsigned char> bytes(HeaderBytes);
    PutInteger(bytes, 0, SAVEFILE_HEADER);
    PutInteger(bytes, 1, SAVEGAME_VERSION);
    PutInteger(bytes, 2, sizeof(data));
    PutInteger(bytes, 3, 2);
    PutInteger(bytes, 4, 3);
    PutInteger(bytes, 5, sizeof(tokens));
    bytes.insert(bytes.end(), std::begin(tokens), std::end(tokens));
    bytes.insert(bytes.end(), std::begin(data), std::end(data));
    return bytes;
}

bool CheckLayout() {
    input = {MakeSave()};
    auto *save = LoadSaveData("c1a0");
    if (!save) {
        return false;
    }
    const bool valid = reinterpret_cast<std::uintptr_t>(save->pTable) % alignof(ENTITYTABLE) == 0
        && save->tableCount == 2 && save->tokenCount == 3 && save->tokenSize == 12 && save->bufferSize == 3
        && save->pTokens && std::strcmp(save->pTokens[0], "alpha") == 0 && save->pTokens[1] == nullptr
        && std::strcmp(save->pTokens[2], "beta") == 0 && save->pCurrentData == save->pBaseData
        && std::memcmp(save->pBaseData, input.bytes.data() + HeaderBytes + save->tokenSize, 3) == 0
        && input.closes == 1 && gGlobalVariables.pSaveData == save;
    FreeSave(save);
    return valid && allocations.empty();
}

bool Rejected(std::vector<unsigned char> bytes) {
    input = {std::move(bytes)};
    auto *save = LoadSaveData("c1a0");
    const bool rejected = !save && allocations.empty() && input.opens == 1 && input.closes == 1
        && gGlobalVariables.pSaveData == nullptr;
    FreeSave(save);
    return rejected;
}

bool CheckMalformed() {
    const auto valid = MakeSave();
    auto bad = valid;
    PutInteger(bad, 0, 0);
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    PutInteger(bad, 1, SAVEGAME_VERSION + 1);
    if (!Rejected(bad)) {
        return false;
    }
    for (std::size_t size = 0; size < valid.size(); ++size) {
        if (!Rejected({valid.begin(), valid.begin() + size})) {
            std::fprintf(stderr, "Accepted truncated save at %zu bytes\n", size);
            return false;
        }
    }
    for (const auto index : {2, 3, 4, 5}) {
        bad = valid;
        PutInteger(bad, index, -1);
        if (!Rejected(bad)) {
            return false;
        }
        bad = valid;
        PutInteger(bad, index, INT_MAX);
        if (!Rejected(bad)) {
            return false;
        }
    }
    bad = valid;
    std::fill(bad.begin() + HeaderBytes, bad.end(), 'x');
    if (!Rejected(bad)) {
        return false;
    }
    bad = valid;
    PutInteger(bad, 4, 2); // The declared token pool must not silently move the payload.
    if (!Rejected(bad)) {
        return false;
    }
    for (const auto fail : {0, 1}) {
        failAllocation = fail;
        allocationAttempt = 0;
        const bool rejected = Rejected(valid);
        failAllocation = -1;
        if (!rejected) {
            return false;
        }
    }
    return true;
}

bool CheckEmptySave() {
    std::vector<unsigned char> bytes(HeaderBytes);
    PutInteger(bytes, 0, SAVEFILE_HEADER);
    PutInteger(bytes, 1, SAVEGAME_VERSION);
    input = {bytes};
    auto *save = LoadSaveData("empty");
    const bool valid = save && save->tableCount == 0 && save->tokenCount == 0 && save->tokenSize == 0
        && save->pTokens == nullptr && save->bufferSize == 0 && input.closes == 1;
    FreeSave(save);
    return valid && allocations.empty();
}
} // namespace

int main(int argc, char **argv) {
    if (!CheckLayout()) {
        std::fputs("Native save table alignment, tokens or payload failed\n", stderr);
        return 1;
    }
    if (!CheckMalformed() || !CheckEmptySave()) {
        std::fputs("Malformed save or allocation lifecycle failed\n", stderr);
        return 1;
    }
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            return 1;
        }
        input = {std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), {})};
        auto *save = LoadSaveData("c1a0");
        if (!save || reinterpret_cast<std::uintptr_t>(save->pTable) % alignof(ENTITYTABLE) != 0) {
            FreeSave(save);
            return 1;
        }
        std::printf("Legacy corpus: %d entities, %d tokens, %d payload bytes\n",
            save->tableCount, save->tokenCount, save->bufferSize);
        FreeSave(save);
    }
    std::puts("Save arena alignment, envelope bounds, tokens and allocation lifecycle passed");
    return 0;
}
