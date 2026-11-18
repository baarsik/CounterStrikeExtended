#pragma once

#include "../../public/cse_server_forms.h"

#include <filesystem>
#include <optional>

namespace cse::forms {

// The caller supplies the engine's connected peer. Asset names and paths from the server never reach the cache.
class AssetCache {
public:
    AssetCache(std::filesystem::path root, std::string_view peer);
    std::optional<Image> Load(AssetKind kind, const Digest& digest, const Digest& proof,
        const Nonce& nonce, std::size_t length) const;
    void Store(AssetKind kind, const Digest& digest, const Nonce& nonce, std::string_view bytes) const;

private:
    std::filesystem::path root_;
    std::filesystem::path scope_;
};

} // namespace cse::forms
