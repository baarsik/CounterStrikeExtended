#ifndef CSE_VGUI_LOCALIZATION_CATALOGUE_H
#define CSE_VGUI_LOCALIZATION_CATALOGUE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cse {
namespace localization {

struct Catalogue {
    std::string language;
    std::vector<std::pair<std::string, std::wstring>> tokens;
};

// The VGUI ABI stores UTF16 cells in wchar_t even on native four-byte-wchar hosts.
// Parse bounded disk cells directly rather than truncating stack token buffers.
class CatalogueParser {
public:
    bool Parse(const std::vector<unsigned char>& bytes, Catalogue& catalogue) {
        constexpr std::size_t CellBytes = 2;
        if (bytes.size() < CellBytes || bytes.size() % CellBytes != 0
            || bytes[0] != 0xff || bytes[1] != 0xfe) {
            return false;
        }
        cells_.clear();
        position_ = 0;
        failed_ = false;
        bool terminator = false;
        constexpr unsigned int BitsPerByte = 8;
        for (std::size_t offset = CellBytes; offset < bytes.size(); offset += CellBytes) {
            const auto cell = static_cast<std::uint16_t>(bytes[offset]
                | (static_cast<unsigned int>(bytes[offset + 1]) << BitsPerByte));
            if (cell == 0) {
                terminator = true;
            } else if (terminator) {
                return false;
            } else {
                cells_.push_back(cell);
            }
        }
        constexpr std::uint16_t FirstHighSurrogate = 0xd800;
        constexpr std::uint16_t LastHighSurrogate = 0xdbff;
        constexpr std::uint16_t FirstLowSurrogate = 0xdc00;
        constexpr std::uint16_t LastLowSurrogate = 0xdfff;
        for (std::size_t index = 0; index < cells_.size(); ++index) {
            const auto cell = cells_[index];
            if (cell >= FirstHighSurrogate && cell <= LastHighSurrogate) {
                if (++index == cells_.size() || cells_[index] < FirstLowSurrogate
                    || cells_[index] > LastLowSurrogate) {
                    return false;
                }
            } else if (cell >= FirstLowSurrogate && cell <= LastLowSurrogate) {
                return false;
            }
        }
        catalogue = Catalogue{};
        const Token root = Next();
        if (!Equal(root.text, L"lang") || !Take(Kind::Open)) {
            return false;
        }
        bool haveTokens = false;
        while (!failed_) {
            const Token key = Next();
            if (key.kind == Kind::Close) {
                const Token trailing = Next();
                return haveTokens && !failed_ && trailing.kind == Kind::End;
            }
            if (key.kind != Kind::Text) {
                return false;
            }
            if (Equal(key.text, L"Tokens")) {
                if (!Take(Kind::Open)) {
                    return false;
                }
                haveTokens = true;
                for (;;) {
                    const Token name = Next();
                    if (name.kind == Kind::Close) {
                        break;
                    }
                    const Token value = Next();
                    std::string utf8;
                    if (name.kind != Kind::Text || name.text.empty() || value.kind != Kind::Text
                        || !Utf8(name.text, utf8)) {
                        return false;
                    }
                    catalogue.tokens.emplace_back(std::move(utf8), value.text);
                }
            } else {
                const Token value = Next();
                if (value.kind != Kind::Text) {
                    return false;
                }
                if (Equal(key.text, L"Language") && !Utf8(value.text, catalogue.language)) {
                    return false;
                }
            }
        }
        return false;
    }

private:
    enum class Kind { Text, Open, Close, End };
    struct Token {
        Kind kind = Kind::End;
        std::wstring text;
    };

    static bool Equal(const std::wstring& first, const wchar_t* second) {
        std::size_t index = 0;
        for (; index < first.size() && second[index]; ++index) {
            wchar_t cell = first[index];
            wchar_t other = second[index];
            constexpr wchar_t CaseOffset = L'a' - L'A';
            if (cell >= L'A' && cell <= L'Z') {
                cell += CaseOffset;
            }
            if (other >= L'A' && other <= L'Z') {
                other += CaseOffset;
            }
            if (cell != other) {
                return false;
            }
        }
        return index == first.size() && second[index] == 0;
    }

    static bool Utf8(const std::wstring& cells, std::string& output) {
        output.clear();
        for (std::size_t index = 0; index < cells.size(); ++index) {
            std::uint32_t scalar = static_cast<std::uint32_t>(cells[index]);
            constexpr std::uint32_t HighSurrogate = 0xd800;
            constexpr std::uint32_t LowSurrogate = 0xdc00;
            constexpr std::uint32_t SurrogateEnd = 0xe000;
            constexpr std::uint32_t SupplementaryBase = 0x10000;
            constexpr unsigned int SurrogateBits = 10;
            if (scalar >= HighSurrogate && scalar < LowSurrogate) {
                if (++index >= cells.size()) {
                    return false;
                }
                const auto low = static_cast<std::uint32_t>(cells[index]);
                if (low < LowSurrogate || low >= SurrogateEnd) {
                    return false;
                }
                scalar = SupplementaryBase + ((scalar - HighSurrogate) << SurrogateBits) + low - LowSurrogate;
            }
            constexpr std::uint32_t SingleByteLimit = 0x80;
            constexpr std::uint32_t TwoByteLimit = 0x800;
            constexpr std::uint32_t ThreeByteLimit = 0x10000;
            constexpr unsigned int PayloadBits = 6;
            constexpr std::uint32_t PayloadMask = 0x3f;
            const auto continuation = [&](std::uint32_t value) {
                output.push_back(static_cast<char>(0x80 | (value & PayloadMask)));
            };
            if (scalar < SingleByteLimit) {
                output.push_back(static_cast<char>(scalar));
            } else if (scalar < TwoByteLimit) {
                output.push_back(static_cast<char>(0xc0 | (scalar >> PayloadBits)));
                continuation(scalar);
            } else if (scalar < ThreeByteLimit) {
                output.push_back(static_cast<char>(0xe0 | (scalar >> (2 * PayloadBits))));
                continuation(scalar >> PayloadBits);
                continuation(scalar);
            } else {
                output.push_back(static_cast<char>(0xf0 | (scalar >> (3 * PayloadBits))));
                continuation(scalar >> (2 * PayloadBits));
                continuation(scalar >> PayloadBits);
                continuation(scalar);
            }
        }
        return true;
    }

    bool Take(Kind kind) {
        return Next().kind == kind && !failed_;
    }

    Token Next() {
        while (position_ < cells_.size()) {
            const auto cell = cells_[position_];
            if (cell == ' ' || cell == '\t' || cell == '\r' || cell == '\n') {
                ++position_;
            } else if (cell == '/' && position_ + 1 < cells_.size() && cells_[position_ + 1] == '/') {
                while (position_ < cells_.size() && cells_[position_] != '\n') {
                    ++position_;
                }
            } else {
                break;
            }
        }
        if (position_ == cells_.size()) {
            return {};
        }
        const auto first = cells_[position_++];
        if (first == '{') {
            return {Kind::Open, {}};
        }
        if (first == '}') {
            return {Kind::Close, {}};
        }
        Token token{Kind::Text, {}};
        const bool quoted = first == '"';
        if (!quoted) {
            token.text.push_back(first);
        }
        while (position_ < cells_.size()) {
            auto cell = cells_[position_];
            if (quoted && cell == '"') {
                ++position_;
                return token;
            }
            if (!quoted && (cell == ' ' || cell == '\t' || cell == '\r' || cell == '\n'
                || cell == '{' || cell == '}')) {
                return token;
            }
            ++position_;
            if (quoted && cell == '\\' && position_ < cells_.size()) {
                const auto next = cells_[position_];
                if (next == 'n' || next == '"') {
                    cell = next == 'n' ? '\n' : next;
                    ++position_;
                }
            }
            token.text.push_back(cell);
        }
        failed_ = quoted;
        return token;
    }

    std::vector<std::uint16_t> cells_;
    std::size_t position_ = 0;
    bool failed_ = false;
};

} // namespace localization
} // namespace cse

#endif
