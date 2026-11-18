#include "../../../src/cstrike/cl_dll/server_form_cache.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

using namespace cse::forms;

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::path(argv[1]) / ("cache-test-" + std::to_string(run));
    const std::string bytes("\1\0\1\0\xff\xff\0\xff", 8);
    const auto digest = Hash(bytes);
    Nonce nonce{};
    nonce[0] = 5;
    const auto proof = MakeProof(nonce, bytes);
    constexpr auto kind = AssetKind::PersistentImage;
    AssetCache original(root, "127.0.0.1:27015");
    assert(!original.Load(kind, digest, proof, nonce, bytes.size()));
    original.Store(kind, digest, nonce, bytes);

    AssetCache reconnect(root, "127.0.0.1:27015");
    auto cached = reconnect.Load(kind, digest, proof, nonce, bytes.size());
    assert(cached && cached->pixels.size() == 4 && cached->pixels[1] == 255);
    AssetCache otherServer(root, "127.0.0.1:27016");
    assert(!otherServer.Load(kind, digest, proof, nonce, bytes.size()));
    nonce[0] = 6;
    assert(!reconnect.Load(kind, digest, proof, nonce, bytes.size()));
    assert(!reconnect.Load(kind, digest, MakeProof(nonce, bytes), nonce, 9));
    assert(reconnect.Load(kind, digest, MakeProof(nonce, bytes), nonce, bytes.size()));
    std::filesystem::remove_all(root);
}
