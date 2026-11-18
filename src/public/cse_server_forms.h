#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cse::forms {

inline constexpr char kUserMessage[] = "CseForm";
inline constexpr char kCapabilityKey[] = "*cseforms";
inline constexpr char kCapabilityVersion[] = "1";

constexpr bool CanAppendCapability(std::size_t userinfoBytes, std::size_t capacity) {
    // The two array terminators account for the separators before the key and value.
    constexpr auto addition = sizeof(kCapabilityKey) + sizeof(kCapabilityVersion);
    return capacity > addition && userinfoBytes < capacity - addition;
}

inline constexpr unsigned kMaxAssets = 32;
inline constexpr unsigned kMaxFormBytes = 8192;
inline constexpr unsigned kMaxImageSide = 64;
inline constexpr unsigned kMaxImagePixelBytes = kMaxImageSide * kMaxImageSide * 4;
inline constexpr unsigned kMaxImageBytes = 4 + kMaxImagePixelBytes;
inline constexpr unsigned kMaxBundleBytes = 128 * 1024;
inline constexpr unsigned kMaxChunkBytes = 160; // Leaves room for the header within GoldSrc's 192-byte limit.
inline constexpr unsigned kMaxValueBytes = 48;
inline constexpr unsigned kMaxNameBytes = 24;
inline constexpr unsigned kMaxControls = 48;

enum class Op : std::uint8_t { Hello = 1, Offer, Chunk, Show, Hide };
enum class AssetKind : std::uint8_t { Form = 1, Image, PersistentImage, WebPImage, PersistentWebPImage };
enum class ControlType { Label, Button, Check, Radio, Slider, Select, Text, Image, Divider, Group };

constexpr bool IsWebPImage(AssetKind kind) {
    return kind == AssetKind::WebPImage || kind == AssetKind::PersistentWebPImage;
}

constexpr bool IsPersistentImage(AssetKind kind) {
    return kind == AssetKind::PersistentImage || kind == AssetKind::PersistentWebPImage;
}

constexpr bool IsImage(AssetKind kind) {
    return kind == AssetKind::Image || kind == AssetKind::PersistentImage || IsWebPImage(kind);
}

using Digest = std::array<std::uint8_t, 32>;
using Nonce = std::array<std::uint8_t, 16>;

struct Control {
    ControlType type{};
    int x{};
    int y{};
    int width{};
    int height{};
    char key{};
    int first{};
    int second{};
    std::string text;
    std::vector<std::string> options;
};

struct Form {
    int width{};
    int height{};
    std::string title;
    std::vector<Control> controls;
};

struct Image {
    int width{};
    int height{};
    std::vector<std::uint8_t> pixels;
};

Digest Hash(std::string_view bytes);
Digest MakeProof(const Nonce& nonce, std::string_view bytes);
bool DecodeHex(std::string_view text, std::uint8_t* output, std::size_t bytes);
std::string EncodeHex(const std::uint8_t* data, std::size_t bytes);
bool IsName(std::string_view name);
bool ParseForm(std::string_view text, Form& form);
bool ParseImage(std::string_view bytes, Image& image);
bool ParseImage(AssetKind kind, std::string_view bytes, Image& image);
bool ValidAction(const Form& form, unsigned control, std::string_view value);

} // namespace cse::forms
