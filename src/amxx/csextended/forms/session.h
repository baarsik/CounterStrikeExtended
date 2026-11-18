#ifndef CSE_FORMS_SERVER_SESSION_H
#define CSE_FORMS_SERVER_SESSION_H

#include "cse_server_forms.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cse::forms::server {

struct Asset {
    std::string name;
    AssetKind kind;
    std::string bytes;
    Digest digest;
    Form form;
};

using Bundle = std::vector<Asset>;

struct Message {
    // GoldSrc limits variable-length user messages to 192 payload bytes.
    std::array<std::uint8_t, 192> bytes{};
    std::size_t size = 0;
};

class Session {
public:
    void Reset();
    void Begin();
    bool AcceptHello(std::string_view nonce);
    bool AcceptAsset(const Bundle& bundle, unsigned id, std::string_view status);
    Message Tick(const Bundle& bundle, float now);
    bool Ready(const Bundle& bundle) const;
    bool Show(const Bundle& bundle, std::string_view name);
    bool Hide();
    const Asset* Action(const Bundle& bundle, std::uint32_t token, unsigned control, std::string_view value, float now);

private:
    enum class Phase { Disabled, SendHello, WaitHello, Offer, WaitRequest, SendChunks, WaitResult };
    static constexpr float ActionBurst = 12.0f;

    void AdvanceAsset();

    Phase phase_ = Phase::Disabled;
    Nonce nonce_{};
    std::array<bool, kMaxAssets> available_{};
    std::size_t assetIndex_ = 0;
    std::size_t offset_ = 0;
    float nextSend_ = 0.0f;
    float replyDeadline_ = 0.0f;
    float actionCredits_ = ActionBurst;
    float lastAction_ = 0.0f;
    std::uint32_t nextToken_ = 0;
    std::uint32_t shownToken_ = 0;
    // Asset IDs are one-based. Zero means that no form should be visible.
    unsigned shownForm_ = 0;
    unsigned wantedForm_ = 0;
    bool viewChanged_ = false;
};

} // namespace cse::forms::server

#endif
