#include "cse_server_forms.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <utility>

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

constexpr unsigned kMaxLabelBytes = 160;
constexpr unsigned kMaxOptions = 8;
constexpr int kMinimumWidth = 240;
constexpr int kMaximumWidth = 800;
constexpr int kMinimumHeight = 180;
constexpr int kMaximumHeight = 600;
constexpr int kMaxSliderValue = 10000;
constexpr unsigned kRadioGroups = 9;
constexpr char hexDigits[] = "0123456789abcdef";

bool ReadInteger(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool Printable(std::string_view utf8, std::size_t limit) {
    if (utf8.size() > limit) {
        return false;
    }
    while (!utf8.empty()) {
        const auto lead = static_cast<std::uint8_t>(utf8.front());
        unsigned count = 1;
        unsigned codePoint = lead;
        unsigned minimum = 0;
        if (lead >= 0xf0 && lead <= 0xf4) {
            count = 4;
            codePoint = lead & 7U;
            minimum = 0x10000;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            count = 3;
            codePoint = lead & 15U;
            minimum = 0x800;
        } else if (lead >= 0xc2 && lead <= 0xdf) {
            count = 2;
            codePoint = lead & 31U;
            minimum = 0x80;
        } else if (lead > 0x7f) {
            return false;
        }
        if (utf8.size() < count) {
            return false;
        }
        for (unsigned i = 1; i < count; ++i) {
            const auto next = static_cast<std::uint8_t>(utf8[i]);
            if ((next & 0xc0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3fU);
        }
        // Reject control characters, malformed UTF-8 and bidirectional override/isolate characters.
        if (codePoint < minimum || codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f) ||
            (codePoint >= 0xd800 && codePoint <= 0xdfff) || codePoint > 0x10ffff ||
            (codePoint >= 0x2028 && codePoint <= 0x202e) || (codePoint >= 0x2066 && codePoint <= 0x2069)) {
            return false;
        }
        utf8.remove_prefix(count);
    }
    return true;
}

std::string_view Next(std::string_view& input, char delimiter = ' ') {
    const auto position = input.find(delimiter);
    const auto part = input.substr(0, position);
    input.remove_prefix(position == input.npos ? input.size() : position + 1);
    return part;
}

char Key(char key) {
    if (key >= 'A' && key <= 'Z') {
        key = static_cast<char>(key + ('a' - 'A'));
    }
    return key;
}

bool Interactive(ControlType type) {
    return type == ControlType::Button || type == ControlType::Check || type == ControlType::Radio ||
        type == ControlType::Slider || type == ControlType::Select || type == ControlType::Text;
}

bool ParseOptions(std::string_view options, Control& control) {
    do {
        const auto separator = options.find('|');
        const auto option = Next(options, '|');
        if (option.empty() || control.options.size() == kMaxOptions || !Printable(option, kMaxValueBytes)) {
            return false;
        }
        control.options.emplace_back(option);
        if (separator == options.npos) {
            break;
        }
    } while (true);
    return control.first >= 0 && static_cast<std::size_t>(control.first) < control.options.size() && control.second == 0;
}

bool ParseControl(std::string_view line, const Form& form, Control& control) {
    constexpr std::array<std::string_view, 10> names{
        "label", "button", "check", "radio", "slider", "select", "text", "image", "divider", "group"};
    const auto name = Next(line);
    const auto type = std::find(names.begin(), names.end(), name);
    if (type == names.end()) {
        return false;
    }
    control.type = static_cast<ControlType>(type - names.begin());
    if (!ReadInteger(Next(line), control.x) || !ReadInteger(Next(line), control.y) ||
        !ReadInteger(Next(line), control.width) || !ReadInteger(Next(line), control.height) ||
        control.x < 0 || control.y < 0 || control.width < 1 || control.height < 1 ||
        control.x > form.width - control.width || control.y > form.height - control.height) {
        return false;
    }
    const auto hotkey = Next(line);
    if (hotkey.size() != 1 || !ReadInteger(Next(line), control.first) || !ReadInteger(Next(line), control.second)) {
        return false;
    }
    if (hotkey.front() != '-') {
        control.key = Key(hotkey.front());
        if (!Interactive(control.type) || !((control.key >= 'a' && control.key <= 'z') ||
            (control.key >= '0' && control.key <= '9'))) {
            return false;
        }
    }
    if (control.type == ControlType::Select) {
        return ParseOptions(line, control);
    }
    if (!Printable(line, control.type == ControlType::Text ? kMaxValueBytes : kMaxLabelBytes)) {
        return false;
    }
    control.text = line;
    switch (control.type) {
        case ControlType::Check:
            return (control.first == 0 || control.first == 1) && control.second == 0 && !line.empty();
        case ControlType::Radio:
            return control.first >= 1 && control.first <= static_cast<int>(kRadioGroups) &&
                (control.second == 0 || control.second == 1) && !line.empty();
        case ControlType::Slider:
            return control.first >= 0 && control.second > control.first && control.second <= kMaxSliderValue;
        case ControlType::Image:
            return control.first == 0 && control.second == 0 && IsName(line);
        default:
            return control.first == 0 && control.second == 0 &&
                (!line.empty() || control.type == ControlType::Text || control.type == ControlType::Divider);
    }
}

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

bool ParseForm(std::string_view text, Form& form) {
    if (text.size() > kMaxFormBytes) {
        return false;
    }
    Form parsed;
    auto header = Next(text, '\n');
    if (!header.empty() && header.back() == '\r') {
        header.remove_suffix(1);
    }
    if (Next(header) != "form" || !ReadInteger(Next(header), parsed.width) ||
        !ReadInteger(Next(header), parsed.height) || parsed.width < kMinimumWidth || parsed.width > kMaximumWidth ||
        parsed.height < kMinimumHeight || parsed.height > kMaximumHeight || header.empty() ||
        !Printable(header, kMaxLabelBytes)) {
        return false;
    }
    parsed.title = header;
    std::array<bool, 128> keys{};
    std::array<bool, kRadioGroups> selectedRadio{};
    while (!text.empty()) {
        auto line = Next(text, '\n');
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        Control control;
        if (parsed.controls.size() == kMaxControls || !ParseControl(line, parsed, control)) {
            return false;
        }
        if (control.key && std::exchange(keys[static_cast<unsigned char>(control.key)], true)) {
            return false;
        }
        if (control.type == ControlType::Radio && control.second && std::exchange(selectedRadio[control.first - 1], true)) {
            return false;
        }
        parsed.controls.push_back(std::move(control));
    }
    if (parsed.controls.empty()) {
        return false;
    }
    form = std::move(parsed);
    return true;
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

bool ValidAction(const Form& form, unsigned control, std::string_view value) {
    if (!control || control > form.controls.size() || value.size() > kMaxValueBytes) {
        return false;
    }
    const auto& item = form.controls[control - 1];
    int number = 0;
    switch (item.type) {
        case ControlType::Button: return value.empty();
        case ControlType::Check: return value == "0" || value == "1";
        case ControlType::Radio: return value == "1";
        case ControlType::Slider: return ReadInteger(value, number) && number >= item.first && number <= item.second;
        case ControlType::Select:
            return ReadInteger(value, number) && number >= 0 && static_cast<std::size_t>(number) < item.options.size();
        case ControlType::Text: return Printable(value, kMaxValueBytes);
        default: return false;
    }
}

} // namespace cse::forms
