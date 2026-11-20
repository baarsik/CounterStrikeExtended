#pragma once

#include "server_form_cache.h"

#include <chrono>
#include <variant>

namespace cse::forms {

enum class AssetStatus { Ignored, Need, Ready, Reject };

struct AssetReply {
    AssetStatus status{AssetStatus::Ignored};
    unsigned id{};
};

struct View {
    std::uint32_t token{};
    unsigned id{};
    std::uint32_t menuRequest{};
};

class Presentation {
public:
    using Time = std::chrono::steady_clock::time_point;

    bool Queue(View view, Time now);
    View TakeReady(Time now);
    void Cancel(std::uint32_t token);
    void Dismiss(Time now);

private:
    View pending_;
    std::uint32_t lastToken_{};
    Time nextShow_{(Time::min)()};
    Time suppressedUntil_{(Time::min)()};
};

class Session {
public:
    void Begin(const Nonce& nonce, std::filesystem::path cacheRoot, std::string_view peer);
    void Reset();
    bool Active() const { return cache_.has_value(); }
    AssetReply Receive(std::string_view packet);
    bool ReceiveMenuBindings(std::string_view packet);
    unsigned MenuForm(StockMenu menu) const;
    const Form* FindForm(unsigned id) const;
    const Image* FindImage(std::string_view name) const;

private:
    struct Asset {
        std::string name;
        std::variant<std::monostate, Form, Image> content;
        bool offered{};
    };
    struct Download {
        unsigned id{};
        AssetKind kind{};
        std::size_t length{};
        Digest digest{};
        Digest proof{};
        std::string bytes;
    };

    AssetReply ReceiveOffer(std::string_view packet);
    AssetReply ReceiveChunk(std::string_view packet);

    std::array<Asset, kMaxAssets> assets_;
    MenuBindings menuBindings_{};
    std::optional<AssetCache> cache_;
    std::optional<Download> download_;
    Nonce nonce_{};
    std::size_t offeredBytes_{};
};

} // namespace cse::forms
