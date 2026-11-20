#include "cse_server_forms.h"

#include <algorithm>
#include <cstring>

namespace cse::forms {
namespace {

// SHA-256 compression constants from FIPS 180-4, section 4.2.2.
constexpr std::array<std::uint32_t, 64> kRounds{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};
constexpr unsigned kBlockBytes = 64;

std::uint32_t RotateRight(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32 - bits));
}

class Sha256 {
public:
    void Add(std::string_view bytes) {
        byteCount_ += bytes.size();
        while (!bytes.empty()) {
            const auto count = std::min(bytes.size(), block_.size() - used_);
            std::memcpy(block_.data() + used_, bytes.data(), count);
            bytes.remove_prefix(count);
            used_ += count;
            if (used_ == block_.size()) {
                Compress();
                used_ = 0;
            }
        }
    }

    Digest Finish() {
        const std::uint64_t bits = byteCount_ * 8;
        constexpr std::size_t lengthPosition = kBlockBytes - sizeof(bits);
        block_[used_++] = 0x80;
        if (used_ > lengthPosition) {
            std::fill(block_.begin() + used_, block_.end(), std::uint8_t{0});
            Compress();
            used_ = 0;
        }
        std::fill(block_.begin() + used_, block_.begin() + lengthPosition, std::uint8_t{0});
        for (unsigned i = 0; i < sizeof(bits); ++i) {
            block_[block_.size() - 1 - i] = static_cast<std::uint8_t>(bits >> (8 * i));
        }
        Compress();
        Digest digest{};
        for (unsigned i = 0; i < state_.size(); ++i) {
            for (unsigned j = 0; j < sizeof(state_[i]); ++j) {
                digest[i * sizeof(state_[i]) + j] = static_cast<std::uint8_t>(state_[i] >> (24 - 8 * j));
            }
        }
        return digest;
    }

private:
    void Compress() {
        std::array<std::uint32_t, kRounds.size()> words{};
        constexpr unsigned blockWords = kBlockBytes / sizeof(std::uint32_t);
        for (unsigned i = 0; i < blockWords; ++i) {
            for (unsigned j = 0; j < sizeof(words[i]); ++j) {
                words[i] = (words[i] << 8) | block_[i * sizeof(words[i]) + j];
            }
        }
        for (unsigned i = blockWords; i < words.size(); ++i) {
            const auto x = words[i - 15];
            const auto y = words[i - 2];
            const auto small0 = RotateRight(x, 7) ^ RotateRight(x, 18) ^ (x >> 3);
            const auto small1 = RotateRight(y, 17) ^ RotateRight(y, 19) ^ (y >> 10);
            words[i] = small1 + words[i - 7] + small0 + words[i - 16];
        }

        auto [a, b, c, d, e, f, g, h] = state_;
        for (unsigned i = 0; i < words.size(); ++i) {
            const auto choose = (e & f) ^ (~e & g);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto sum0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
            const auto sum1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
            const auto first = h + sum1 + choose + kRounds[i] + words[i];
            const auto second = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + first;
            d = c;
            c = b;
            b = a;
            a = first + second;
        }
        const std::array result{a, b, c, d, e, f, g, h};
        for (unsigned i = 0; i < state_.size(); ++i) {
            state_[i] += result[i];
        }
    }

    // SHA-256 initial hash values from FIPS 180-4, section 5.3.3.
    std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, kBlockBytes> block_{};
    std::uint64_t byteCount_{};
    std::size_t used_{};
};

constexpr char hexDigits[] = "0123456789abcdef";

} // namespace

StockMenu TextStockMenu(std::string_view text) {
    if (text == "#Team_Select" || text == "#Team_Select_Spect" || text == "#IG_Team_Select" ||
        text == "#IG_Team_Select_Spect" || text == "#IG_VIP_Team_Select" || text == "#IG_VIP_Team_Select_Spect") {
        return StockMenu::Team;
    }
    if (text == "#Terrorist_Select") {
        return StockMenu::TerroristClass;
    }
    if (text == "#CT_Select") {
        return StockMenu::CounterTerroristClass;
    }
    if (text == "#Buy") {
        return StockMenu::Buy;
    }
    if (text == "#CT_BuyItem" || text == "#T_BuyItem" || text == "#DCT_BuyItem" || text == "#DT_BuyItem") {
        return StockMenu::BuyEquipment;
    }
    return StockMenu::None;
}

Digest Hash(std::string_view bytes) {
    Sha256 hash;
    hash.Add(bytes);
    return hash.Finish();
}

Digest MakeProof(const Nonce& nonce, std::string_view bytes) {
    std::array<char, kBlockBytes> inner{};
    std::array<char, kBlockBytes> outer{};
    // HMAC pads from RFC 2104. The fresh nonce is the HMAC key.
    inner.fill(0x36);
    outer.fill(0x5c);
    for (unsigned i = 0; i < nonce.size(); ++i) {
        inner[i] ^= static_cast<char>(nonce[i]);
        outer[i] ^= static_cast<char>(nonce[i]);
    }
    Sha256 hash;
    hash.Add({inner.data(), inner.size()});
    hash.Add(bytes);
    const auto digest = hash.Finish();
    Sha256 result;
    result.Add({outer.data(), outer.size()});
    result.Add({reinterpret_cast<const char*>(digest.data()), digest.size()});
    return result.Finish();
}

bool DecodeHex(std::string_view text, std::uint8_t* output, std::size_t bytes) {
    if (text.size() % 2 || text.size() / 2 != bytes) {
        return false;
    }
    constexpr std::string_view digits(hexDigits);
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto upper = digits.find(text[i * 2]);
        const auto lower = digits.find(text[i * 2 + 1]);
        if (upper == digits.npos || lower == digits.npos) {
            return false;
        }
        output[i] = static_cast<std::uint8_t>((upper << 4) | lower);
    }
    return true;
}

std::string EncodeHex(const std::uint8_t* data, std::size_t bytes) {
    std::string text;
    text.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        text += hexDigits[data[i] >> 4];
        text += hexDigits[data[i] & 15U];
    }
    return text;
}

bool IsName(std::string_view name) {
    return !name.empty() && name.size() <= kMaxNameBytes && std::all_of(name.begin(), name.end(), [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    });
}

bool ParseImage(std::string_view bytes, Image& image) {
    constexpr unsigned headerBytes = 4;
    constexpr unsigned channels = 4;
    if (bytes.size() < headerBytes || bytes.size() > kMaxImageBytes) {
        return false;
    }
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
    const unsigned width = data[0] | (static_cast<unsigned>(data[1]) << 8);
    const unsigned height = data[2] | (static_cast<unsigned>(data[3]) << 8);
    if (!width || !height || width > kMaxImageSide || height > kMaxImageSide ||
        bytes.size() != headerBytes + width * height * channels) {
        return false;
    }
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.assign(data + headerBytes, data + bytes.size());
    return true;
}

} // namespace cse::forms
