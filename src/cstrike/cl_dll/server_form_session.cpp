#include "server_form_session.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace cse::forms {
namespace {

constexpr unsigned kOfferPrefix = 5;
constexpr unsigned kOfferHeader = kOfferPrefix + 2 * sizeof(Digest);
constexpr unsigned kChunkHeader = 4;
constexpr auto kMinimumShowInterval = std::chrono::milliseconds(250);
constexpr auto kDismissQuietPeriod = std::chrono::milliseconds(750);

unsigned Byte(std::string_view packet, unsigned offset) {
    return static_cast<std::uint8_t>(packet[offset]);
}

unsigned Word(std::string_view packet, unsigned offset) {
    return Byte(packet, offset) | (Byte(packet, offset + 1) << 8);
}

} // namespace

bool Presentation::Queue(View view, Time now) {
    if (!view.token || view.token <= lastToken_) {
        return false;
    }
    lastToken_ = view.token;
    pending_ = now < suppressedUntil_ ? View{} : view;
    return true;
}

View Presentation::TakeReady(Time now) {
    if (!pending_.token || now < nextShow_) {
        return {};
    }
    nextShow_ = now + kMinimumShowInterval;
    return std::exchange(pending_, {});
}

void Presentation::Cancel(std::uint32_t token) {
    if (pending_.token == token) {
        pending_ = {};
    }
}

void Presentation::Dismiss(Time now) {
    pending_ = {};
    suppressedUntil_ = now + kDismissQuietPeriod;
}

void Session::Begin(const Nonce& nonce, std::filesystem::path cacheRoot, std::string_view peer) {
    Reset();
    nonce_ = nonce;
    cache_.emplace(std::move(cacheRoot), peer);
}

void Session::Reset() {
    assets_ = {};
    cache_.reset();
    download_.reset();
    nonce_ = {};
    offeredBytes_ = 0;
}

AssetReply Session::Receive(std::string_view packet) {
    if (!Active() || packet.size() < 2) {
        return {};
    }
    switch (static_cast<Op>(Byte(packet, 0))) {
        case Op::Offer: return ReceiveOffer(packet);
        case Op::Chunk: return ReceiveChunk(packet);
        default: return {};
    }
}

AssetReply Session::ReceiveOffer(std::string_view packet) {
    const unsigned id = Byte(packet, 1);
    if (download_ || !id || id > assets_.size() || assets_[id - 1].offered) {
        return {};
    }
    auto& asset = assets_[id - 1];
    asset.offered = true;
    if (packet.size() <= kOfferHeader || packet.size() > kOfferHeader + kMaxNameBytes) {
        return {AssetStatus::Reject, id};
    }
    const auto kind = static_cast<AssetKind>(Byte(packet, 2));
    const auto length = Word(packet, 3);
    const auto name = packet.substr(kOfferHeader);
    const auto maxLength = kind == AssetKind::Form ? kMaxFormBytes : kMaxImageBytes;
    if ((kind != AssetKind::Form && !IsImage(kind)) ||
        !length || length > maxLength || offeredBytes_ + length > kMaxBundleBytes || !IsName(name) ||
        std::any_of(assets_.begin(), assets_.end(), [name](const auto& item) { return item.name == name; })) {
        return {AssetStatus::Reject, id};
    }
    asset.name = name;
    offeredBytes_ += length;
    Download download;
    download.id = id;
    download.kind = kind;
    download.length = length;
    std::memcpy(download.digest.data(), packet.data() + kOfferPrefix, download.digest.size());
    std::memcpy(download.proof.data(), packet.data() + kOfferPrefix + download.digest.size(), download.proof.size());
    if (IsPersistentImage(kind)) {
        auto image = cache_->Load(kind, download.digest, download.proof, nonce_, length);
        if (image) {
            asset.content = std::move(*image);
            return {AssetStatus::Ready, id};
        }
    }
    download.bytes.reserve(length);
    download_.emplace(std::move(download));
    return {AssetStatus::Need, id};
}

AssetReply Session::ReceiveChunk(std::string_view packet) {
    const auto id = Byte(packet, 1);
    if (!download_ || id != download_->id) {
        return {};
    }
    auto& download = *download_;
    if (packet.size() <= kChunkHeader || packet.size() > kChunkHeader + kMaxChunkBytes ||
        Word(packet, 2) != download.bytes.size() || packet.size() - kChunkHeader > download.length - download.bytes.size()) {
        download_.reset();
        return {AssetStatus::Reject, id};
    }
    download.bytes.append(packet.substr(kChunkHeader));
    if (download.bytes.size() != download.length) {
        return {};
    }

    const auto& bytes = download.bytes;
    auto& content = assets_[id - 1].content;
    if (Hash(bytes) == download.digest) {
        if (download.kind == AssetKind::Form) {
            Form form;
            if (ParseForm(bytes, form)) {
                content = std::move(form);
            }
        } else {
            Image image;
            const bool persistent = IsPersistentImage(download.kind);
            if ((!persistent || MakeProof(nonce_, bytes) == download.proof) && ParseImage(download.kind, bytes, image)) {
                if (persistent) {
                    cache_->Store(download.kind, download.digest, nonce_, bytes);
                }
                content = std::move(image);
            }
        }
    }
    download_.reset();
    return {std::holds_alternative<std::monostate>(content) ? AssetStatus::Reject : AssetStatus::Ready, id};
}

const Form* Session::FindForm(unsigned id) const {
    if (!id || id > assets_.size()) {
        return nullptr;
    }
    const auto* form = std::get_if<Form>(&assets_[id - 1].content);
    if (!form || std::any_of(form->controls.begin(), form->controls.end(), [this](const Control& control) {
        return control.type == ControlType::Image && !FindImage(control.text);
    })) {
        return nullptr;
    }
    return form;
}

const Image* Session::FindImage(std::string_view name) const {
    if (name.empty()) {
        return nullptr;
    }
    const auto asset = std::find_if(assets_.begin(), assets_.end(),
        [name](const Asset& item) { return item.name == name; });
    return asset == assets_.end() ? nullptr : std::get_if<Image>(&asset->content);
}

} // namespace cse::forms
