#include "server_form_cache.h"

#include <fstream>
#include <utility>

namespace cse::forms {
namespace {

namespace fs = std::filesystem;
constexpr unsigned kMaxCachedPeers = 128;
constexpr unsigned kMaxCachedImagesPerPeer = 32;

bool IsDirectory(const fs::path& path) {
    std::error_code error;
    return fs::is_directory(fs::symlink_status(path, error));
}

bool IsPlainFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(fs::symlink_status(path, error));
}

bool Missing(const fs::path& path) {
    std::error_code error;
    return fs::symlink_status(path, error).type() == fs::file_type::not_found;
}

bool HasCapacity(const fs::path& path, unsigned limit) {
    std::error_code error;
    fs::directory_iterator entry(path, error);
    unsigned count = 0;
    for (; !error && entry != fs::directory_iterator(); entry.increment(error)) {
        if (++count >= limit) {
            return false;
        }
    }
    return !error;
}

bool Directory(const fs::path& path) {
    if (Missing(path)) {
        std::error_code error;
        fs::create_directory(path, error);
    }
    return IsDirectory(path);
}

std::string CacheName(AssetKind kind, const Digest& digest) {
    return EncodeHex(digest.data(), digest.size()) + (IsWebPImage(kind) ? ".webp" : ".rgba");
}

bool SameProof(const Digest& expected, const Digest& received) {
    unsigned differences = 0;
    for (unsigned i = 0; i < expected.size(); ++i) {
        differences |= expected[i] ^ received[i];
    }
    return differences == 0;
}

} // namespace

AssetCache::AssetCache(fs::path root, std::string_view peer) : root_(std::move(root)) {
    const auto digest = Hash(peer);
    scope_ = root_ / EncodeHex(digest.data(), digest.size());
}

std::optional<Image> AssetCache::Load(
    AssetKind kind, const Digest& digest, const Digest& proof, const Nonce& nonce, std::size_t length) const {
    if (!IsPersistentImage(kind) || length > kMaxImageBytes || !root_.is_absolute() || !IsDirectory(root_) ||
        !IsDirectory(scope_)) {
        return std::nullopt;
    }
    const auto path = scope_ / CacheName(kind, digest);
    std::error_code error;
    if (!IsPlainFile(path) || fs::file_size(path, error) != length || error) {
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    std::string bytes(length, '\0');
    if (!file.read(bytes.data(), static_cast<std::streamsize>(length)) ||
        file.peek() != std::char_traits<char>::eof() || Hash(bytes) != digest ||
        !SameProof(MakeProof(nonce, bytes), proof)) {
        return std::nullopt;
    }
    Image image;
    if (!ParseImage(kind, bytes, image)) {
        return std::nullopt;
    }
    return image;
}

void AssetCache::Store(AssetKind kind, const Digest& digest, const Nonce& nonce, std::string_view bytes) const {
    Image image;
    if (!IsPersistentImage(kind) || !ParseImage(kind, bytes, image) || Hash(bytes) != digest ||
        !root_.is_absolute() || !Directory(root_) ||
        (!IsDirectory(scope_) && (!HasCapacity(root_, kMaxCachedPeers) || !Directory(scope_)))) {
        return;
    }
    const auto filename = CacheName(kind, digest);
    const auto path = scope_ / filename;
    if (!Missing(path) && !IsPlainFile(path)) {
        return;
    }
    if (Missing(path) && !HasCapacity(scope_, kMaxCachedImagesPerPeer)) {
        return;
    }
    const auto partial = scope_ / (filename + "." + EncodeHex(nonce.data(), nonce.size()) + ".tmp");
    if (!Missing(partial)) {
        return;
    }
    std::ofstream file(partial, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    std::error_code error;
    if (file) {
        // A same-name entry always represents the same content. Concurrent connections can publish either copy.
        fs::rename(partial, path, error);
    }
    fs::remove(partial, error);
}

} // namespace cse::forms
