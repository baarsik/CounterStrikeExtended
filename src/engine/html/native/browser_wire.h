#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cse::browser {

// This local provider protocol carries values and browser IDs, never native pointers or C++ object layouts.
constexpr std::size_t HeaderBytes = 24;
constexpr std::uint16_t ProtocolVersion = 2;
enum class Kind : std::uint16_t { Command = 1, Event = 2, Control = 3, Pixels = 4 };

struct Header {
    Kind kind;
    std::uint32_t operation;
    std::uint32_t browser;
    std::uint32_t request;
    std::uint32_t payloadBytes;
};

struct Message {
    Header header{};
    std::vector<std::uint8_t> payload;
};

namespace wire_detail {
inline void Store32(std::uint8_t* output, std::uint32_t value) {
    for (unsigned int index = 0; index < sizeof(value); ++index) {
        output[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

inline std::uint32_t Load32(const std::uint8_t* input) {
    std::uint32_t result = 0;
    for (unsigned int index = 0; index < sizeof(result); ++index) {
        result |= static_cast<std::uint32_t>(input[index]) << (index * 8);
    }
    return result;
}
}

inline std::array<std::uint8_t, HeaderBytes> EncodeHeader(const Header& header) {
    std::array<std::uint8_t, HeaderBytes> output{};
    output[0] = 'C';
    output[1] = 'S';
    output[2] = 'E';
    output[3] = 'B';
    output[4] = static_cast<std::uint8_t>(ProtocolVersion);
    output[5] = static_cast<std::uint8_t>(ProtocolVersion >> 8);
    const auto kind = static_cast<std::uint16_t>(header.kind);
    output[6] = static_cast<std::uint8_t>(kind);
    output[7] = static_cast<std::uint8_t>(kind >> 8);
    wire_detail::Store32(output.data() + 8, header.operation);
    wire_detail::Store32(output.data() + 12, header.browser);
    wire_detail::Store32(output.data() + 16, header.request);
    wire_detail::Store32(output.data() + 20, header.payloadBytes);
    return output;
}

class Writer {
public:
    bool U32(std::uint32_t value) {
        if (!Reserve(sizeof(value))) {
            return false;
        }
        const std::size_t offset = data_.size();
        data_.resize(offset + sizeof(value));
        wire_detail::Store32(data_.data() + offset, value);
        return true;
    }

    bool I32(std::int32_t value) {
        return U32(static_cast<std::uint32_t>(value));
    }

    bool U64(std::uint64_t value) {
        return U32(static_cast<std::uint32_t>(value)) && U32(static_cast<std::uint32_t>(value >> 32));
    }

    bool Boolean(bool value) {
        return U32(value ? 1 : 0);
    }

    bool String(std::string_view value) {
        return Bytes(value.data(), value.size());
    }

    bool Bytes(const void* source, std::size_t count) {
        constexpr std::size_t LengthBytes = sizeof(std::uint32_t);
        if (count > MaxBytes - LengthBytes || !Reserve(count + LengthBytes) || (!source && count != 0)) {
            good_ = false;
            return false;
        }
        U32(static_cast<std::uint32_t>(count));
        if (count != 0) {
            const auto* begin = static_cast<const std::uint8_t*>(source);
            data_.insert(data_.end(), begin, begin + count);
        }
        return true;
    }

    bool Good() const {
        return good_;
    }

    const std::vector<std::uint8_t>& Data() const {
        return data_;
    }

private:
    bool Reserve(std::size_t count) {
        if (!good_ || count > MaxBytes - data_.size()) {
            good_ = false;
        }
        return good_;
    }

    static constexpr std::size_t MaxBytes = std::numeric_limits<std::uint32_t>::max();
    bool good_ = true;
    std::vector<std::uint8_t> data_;
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

    bool U32(std::uint32_t& value) {
        if (!Require(sizeof(value))) {
            return false;
        }
        value = wire_detail::Load32(data_.data() + offset_);
        offset_ += sizeof(value);
        return true;
    }

    bool I32(std::int32_t& value) {
        std::uint32_t bits = 0;
        if (!U32(bits)) {
            return false;
        }
        constexpr std::uint32_t MaxSigned = std::numeric_limits<std::int32_t>::max();
        value = bits <= MaxSigned ? static_cast<std::int32_t>(bits) :
            -1 - static_cast<std::int32_t>(std::numeric_limits<std::uint32_t>::max() - bits);
        return true;
    }

    bool U64(std::uint64_t& value) {
        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!U32(low) || !U32(high)) {
            return false;
        }
        value = low | (static_cast<std::uint64_t>(high) << 32);
        return true;
    }

    bool Boolean(bool& value) {
        std::uint32_t encoded = 0;
        if (!U32(encoded) || encoded > 1) {
            good_ = false;
            return false;
        }
        value = encoded != 0;
        return true;
    }

    bool String(std::string& value) {
        std::uint32_t count = 0;
        if (!U32(count) || !Require(count)) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(data_.data() + offset_), count);
        offset_ += count;
        return true;
    }

    bool Bytes(std::vector<std::uint8_t>& value) {
        std::uint32_t count = 0;
        if (!U32(count) || !Require(count)) {
            return false;
        }
        value.assign(data_.begin() + offset_, data_.begin() + offset_ + count);
        offset_ += count;
        return true;
    }

    // The view remains valid only while the message payload is alive and unchanged.
    bool BytesView(const std::uint8_t*& value, std::size_t& size) {
        std::uint32_t count = 0;
        if (!U32(count) || !Require(count)) {
            return false;
        }
        value = count == 0 ? nullptr : data_.data() + offset_;
        size = count;
        offset_ += count;
        return true;
    }

    bool Finish() const {
        return good_ && offset_ == data_.size();
    }

private:
    bool Require(std::size_t count) {
        if (!good_ || count > data_.size() - offset_) {
            good_ = false;
        }
        return good_;
    }

    const std::vector<std::uint8_t>& data_;
    std::size_t offset_ = 0;
    bool good_ = true;
};

enum class DecodeStatus { NeedMore, Complete, Invalid };

class Decoder {
public:
    explicit Decoder(std::size_t payloadLimit) : payloadLimit_(payloadLimit) {}

    DecodeStatus Feed(const std::uint8_t* bytes, std::size_t count, std::size_t& used, Message& result) {
        used = 0;
        if (invalid_ || (!bytes && count != 0)) {
            invalid_ = true;
            return DecodeStatus::Invalid;
        }
        if (headerUsed_ < HeaderBytes) {
            const std::size_t amount = std::min(count, HeaderBytes - headerUsed_);
            if (amount != 0) {
                std::memcpy(header_.data() + headerUsed_, bytes, amount);
                headerUsed_ += amount;
                used += amount;
            }
            if (headerUsed_ != HeaderBytes) {
                return DecodeStatus::NeedMore;
            }
            if (!ReadHeader()) {
                invalid_ = true;
                return DecodeStatus::Invalid;
            }
            message_.payload.resize(message_.header.payloadBytes);
        }
        const std::size_t amount = std::min(count - used, message_.payload.size() - bodyUsed_);
        if (amount != 0) {
            std::memcpy(message_.payload.data() + bodyUsed_, bytes + used, amount);
            bodyUsed_ += amount;
            used += amount;
        }
        if (bodyUsed_ != message_.payload.size()) {
            return DecodeStatus::NeedMore;
        }
        result = std::move(message_);
        message_ = {};
        headerUsed_ = 0;
        bodyUsed_ = 0;
        return DecodeStatus::Complete;
    }

    void Reset() {
        headerUsed_ = 0;
        bodyUsed_ = 0;
        invalid_ = false;
        message_ = {};
    }

    bool Finish() const {
        return !invalid_ && headerUsed_ == 0 && bodyUsed_ == 0;
    }

private:
    bool ReadHeader() {
        const std::uint16_t version = header_[4] | (static_cast<std::uint16_t>(header_[5]) << 8);
        const std::uint16_t kind = header_[6] | (static_cast<std::uint16_t>(header_[7]) << 8);
        if (header_[0] != 'C' || header_[1] != 'S' || header_[2] != 'E' || header_[3] != 'B' ||
            version != ProtocolVersion || kind < static_cast<std::uint16_t>(Kind::Command) ||
            kind > static_cast<std::uint16_t>(Kind::Pixels)) {
            return false;
        }
        message_.header = {static_cast<Kind>(kind), wire_detail::Load32(header_.data() + 8),
            wire_detail::Load32(header_.data() + 12), wire_detail::Load32(header_.data() + 16),
            wire_detail::Load32(header_.data() + 20)};
        return message_.header.payloadBytes <= payloadLimit_;
    }

    std::size_t payloadLimit_;
    std::array<std::uint8_t, HeaderBytes> header_{};
    std::size_t headerUsed_ = 0;
    std::size_t bodyUsed_ = 0;
    bool invalid_ = false;
    Message message_;
};
}
