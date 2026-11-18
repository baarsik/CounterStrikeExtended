#include "session.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

namespace cse::forms::server {
namespace {

constexpr float MessageInterval = 0.05f;
constexpr float ReplyTimeout = 8.0f;
constexpr float ActionCreditsPerSecond = 20.0f;
constexpr unsigned BitsPerByte = std::numeric_limits<std::uint8_t>::digits;
constexpr unsigned FirstAssetId = 1;
constexpr unsigned CloseControl = 0;

static_assert(kMaxAssets <= std::numeric_limits<std::uint8_t>::max());
static_assert(kMaxFormBytes <= std::numeric_limits<std::uint16_t>::max());
static_assert(kMaxImageBytes <= std::numeric_limits<std::uint16_t>::max());
static_assert(kMaxChunkBytes + 2 * sizeof(std::uint8_t) + sizeof(std::uint16_t) <= Message{}.bytes.size());
static_assert(3 * sizeof(std::uint8_t) + sizeof(std::uint16_t) + 2 * Digest{}.size() + kMaxNameBytes <=
              Message{}.bytes.size());

void AppendBytes(Message& message, const void* source, std::size_t size) {
    assert(size <= message.bytes.size() - message.size);
    std::memcpy(message.bytes.data() + message.size, source, size);
    message.size += size;
}

template <typename Integer>
void AppendInteger(Message& message, Integer value) {
    for (unsigned byte = 0; byte < sizeof(value); ++byte) {
        const auto octet = static_cast<std::uint8_t>(value >> (byte * BitsPerByte));
        AppendBytes(message, &octet, sizeof(octet));
    }
}

void AppendOp(Message& message, Op op) {
    AppendInteger(message, static_cast<std::uint8_t>(op));
}

} // namespace

void Session::Reset() {
    // Keep tokens unique across map changes and reconnects in the same slot.
    const auto token = nextToken_;
    *this = Session{};
    nextToken_ = token;
}

void Session::Begin() {
    Reset();
    phase_ = Phase::SendHello;
}

bool Session::AcceptHello(std::string_view nonce) {
    if (phase_ != Phase::WaitHello || !DecodeHex(nonce, nonce_.data(), nonce_.size())) {
        return false;
    }
    phase_ = Phase::Offer;
    return true;
}

bool Session::AcceptAsset(const Bundle& bundle, unsigned id, std::string_view status) {
    if (assetIndex_ >= bundle.size() || id != assetIndex_ + FirstAssetId) {
        return false;
    }
    if (status == "need" && phase_ == Phase::WaitRequest) {
        phase_ = Phase::SendChunks;
        offset_ = 0;
        return true;
    }
    if (phase_ != Phase::WaitRequest && phase_ != Phase::WaitResult && phase_ != Phase::SendChunks) {
        return false;
    }
    if (status == "reject") {
        AdvanceAsset();
        return true;
    }
    const bool cacheHit = phase_ == Phase::WaitRequest && IsPersistentImage(bundle[assetIndex_].kind);
    if (status == "ready" && (phase_ == Phase::WaitResult || cacheHit)) {
        available_[assetIndex_] = true;
        AdvanceAsset();
        return true;
    }
    return false;
}

void Session::AdvanceAsset() {
    ++assetIndex_;
    phase_ = Phase::Offer;
}

Message Session::Tick(const Bundle& bundle, float now) {
    Message message;
    if (phase_ == Phase::WaitHello && now >= replyDeadline_) {
        phase_ = Phase::Disabled;
    }
    if ((phase_ == Phase::WaitRequest || phase_ == Phase::WaitResult) && now >= replyDeadline_) {
        AdvanceAsset();
    }
    if (now < nextSend_) {
        return message;
    }

    if (viewChanged_) {
        if (wantedForm_ != 0) {
            ++nextToken_;
            if (nextToken_ == 0) {
                ++nextToken_;
            }
            shownToken_ = nextToken_;
            AppendOp(message, Op::Show);
            AppendInteger(message, static_cast<std::uint8_t>(wantedForm_));
            AppendInteger(message, shownToken_);
        } else if (shownForm_ != 0) {
            AppendOp(message, Op::Hide);
            AppendInteger(message, shownToken_);
            shownToken_ = 0;
        }
        shownForm_ = wantedForm_;
        viewChanged_ = false;
    } else if (phase_ == Phase::SendHello) {
        AppendOp(message, Op::Hello);
        phase_ = Phase::WaitHello;
        replyDeadline_ = now + ReplyTimeout;
    } else if (assetIndex_ < bundle.size()) {
        const Asset& asset = bundle[assetIndex_];
        const auto id = static_cast<std::uint8_t>(assetIndex_ + FirstAssetId);
        if (phase_ == Phase::Offer) {
            const Digest proof = IsPersistentImage(asset.kind) ? MakeProof(nonce_, asset.bytes) : Digest{};
            AppendOp(message, Op::Offer);
            AppendInteger(message, id);
            AppendInteger(message, static_cast<std::uint8_t>(asset.kind));
            AppendInteger(message, static_cast<std::uint16_t>(asset.bytes.size()));
            AppendBytes(message, asset.digest.data(), asset.digest.size());
            AppendBytes(message, proof.data(), proof.size());
            AppendBytes(message, asset.name.data(), asset.name.size());
            phase_ = Phase::WaitRequest;
            replyDeadline_ = now + ReplyTimeout;
        } else if (phase_ == Phase::SendChunks) {
            const std::size_t count = std::min<std::size_t>(kMaxChunkBytes, asset.bytes.size() - offset_);
            AppendOp(message, Op::Chunk);
            AppendInteger(message, id);
            AppendInteger(message, static_cast<std::uint16_t>(offset_));
            AppendBytes(message, asset.bytes.data() + offset_, count);
            offset_ += count;
            if (offset_ == asset.bytes.size()) {
                phase_ = Phase::WaitResult;
                replyDeadline_ = now + ReplyTimeout;
            }
        }
    }
    if (message.size != 0) {
        nextSend_ = now + MessageInterval;
    }
    return message;
}

bool Session::Ready(const Bundle& bundle) const {
    return phase_ == Phase::Offer && assetIndex_ == bundle.size();
}

bool Session::Show(const Bundle& bundle, std::string_view name) {
    for (std::size_t index = 0; index < bundle.size(); ++index) {
        const Asset& asset = bundle[index];
        if (asset.kind != AssetKind::Form || asset.name != name || !available_[index]) {
            continue;
        }
        for (const auto& control : asset.form.controls) {
            if (control.type != ControlType::Image) {
                continue;
            }
            bool available = false;
            for (std::size_t image = 0; image < bundle.size(); ++image) {
                if (available_[image] && bundle[image].kind != AssetKind::Form && bundle[image].name == control.text) {
                    available = true;
                    break;
                }
            }
            if (!available) {
                return false;
            }
        }
        wantedForm_ = static_cast<unsigned>(index) + FirstAssetId;
        viewChanged_ = true;
        return true;
    }
    return false;
}

bool Session::Hide() {
    if (wantedForm_ == 0 && shownForm_ == 0) {
        return false;
    }
    viewChanged_ = shownForm_ != 0;
    wantedForm_ = 0;
    return true;
}

const Asset* Session::Action(const Bundle& bundle, std::uint32_t token, unsigned control, std::string_view value,
                             float now) {
    if (viewChanged_ || shownForm_ == 0 || token != shownToken_) {
        return nullptr;
    }
    if (control == CloseControl) {
        if (value.empty()) {
            shownToken_ = 0;
            shownForm_ = 0;
            wantedForm_ = 0;
        }
        return nullptr;
    }
    const Asset& asset = bundle[shownForm_ - FirstAssetId];
    if (!ValidAction(asset.form, control, value)) {
        return nullptr;
    }
    const float elapsed = std::max(0.0f, now - lastAction_);
    const float credits = std::min(ActionBurst, actionCredits_ + elapsed * ActionCreditsPerSecond);
    if (credits < 1.0f) {
        return nullptr;
    }
    actionCredits_ = credits - 1.0f;
    lastAction_ = now;
    return &asset;
}

} // namespace cse::forms::server
