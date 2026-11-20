#include "session.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cse::forms;
using namespace cse::forms::server;

namespace {

void Require(bool passed, const char* message) {
    if (!passed) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

constexpr char NonceHex[] = "000102030405060708090a0b0c0d0e0f";

Asset MakeAsset(std::string name, AssetKind kind, std::string bytes) {
    const auto digest = Hash(bytes);
    return {std::move(name), kind, std::move(bytes), digest, {}};
}

void Negotiate(Session& session, const Bundle& bundle) {
    Require(!session.AcceptHello(NonceHex), "accepting hello before sending one enables unsolicited transfers");
    session.Begin();
    const auto hello = session.Tick(bundle, 0.0f);
    Require(hello.size == 1 && hello.bytes[0] == 1, "hello must be sent as a one-byte message");
    Require(!session.AcceptHello("invalid"), "malformed nonce must be rejected");
    Require(session.AcceptHello(NonceHex), "valid nonce must start synchronization");
    Require(!session.AcceptHello(NonceHex), "second hello must not reset synchronization");
}

void TransfersAreBoundedAndOrdered() {
    const std::string payload = std::string(160, 'x') + "end";
    const Bundle bundle{MakeAsset("banner", AssetKind::Image, payload)};
    Session session;
    Negotiate(session, bundle);
    Require(!session.Ready(bundle), "hello alone must not report the bundle synchronized");
    Require(session.Tick(bundle, 0.02f).size == 0, "follow-up messages must respect frame pacing");

    const auto offer = session.Tick(bundle, 0.06f);
    Require(offer.size == 75 && offer.bytes[0] == 2 && offer.bytes[1] == 1 && offer.bytes[2] == 2 &&
            offer.bytes[3] == 163 && offer.bytes[4] == 0 &&
            std::equal(offer.bytes.begin() + 69, offer.bytes.begin() + 75, "banner"),
            "offer must carry the id, kind, little-endian size and name");
    Require(std::all_of(offer.bytes.begin() + 37, offer.bytes.begin() + 69,
                        [](std::uint8_t byte) { return byte == 0; }),
            "temporary images must have an empty cache proof");
    Require(!session.AcceptAsset(bundle, 1, "ready"), "only a persistent image can be acknowledged from cache");
    Require(!session.AcceptAsset(bundle, 2, "need"), "response for an unoffered asset must be rejected");
    Require(session.AcceptAsset(bundle, 1, "need"), "current temporary image can be requested");
    const auto first = session.Tick(bundle, 0.12f);
    Require(first.size == 164 && first.bytes[0] == 3 && first.bytes[1] == 1 && first.bytes[2] == 0 &&
            first.bytes[3] == 0 && first.bytes[4] == 'x' && first.bytes[163] == 'x',
            "first chunk must start at zero and include at most 160 payload bytes");
    Require(!session.AcceptAsset(bundle, 1, "ready"), "incomplete transient transfer cannot be acknowledged");
    Require(session.Tick(bundle, 0.13f).size == 0, "chunks must respect the reliable message interval");
    const auto last = session.Tick(bundle, 0.18f);
    Require(last.size == 7 && last.bytes[0] == 3 && last.bytes[1] == 1 && last.bytes[2] == 160 &&
            last.bytes[3] == 0 && std::equal(last.bytes.begin() + 4, last.bytes.begin() + 7, "end"),
            "last chunk must carry the remaining bytes at the next offset");
    Require(session.AcceptAsset(bundle, 1, "ready") && session.Ready(bundle),
            "successful final acknowledgement must complete synchronization");
}

void PersistentImageCacheAndTimeout() {
    const Bundle bundle{MakeAsset("banner", AssetKind::PersistentImage, "first"),
                        MakeAsset("unused", AssetKind::Image, "second"),
                        MakeAsset("next", AssetKind::Image, "third")};
    Session session;
    Negotiate(session, bundle);
    const auto cached = session.Tick(bundle, 0.06f);
    Require(cached.bytes[2] == 3 && EncodeHex(cached.bytes.data() + 5, 32) ==
            "a7937b64b8caa58f03721bb6bacf5c78cb235febe0e70b1b84cd99541461a08e" &&
            EncodeHex(cached.bytes.data() + 37, 32) ==
            "ce65d13dca5a4f9ec941a9ac9d6df792a1d39a650de168929f4170a0f9f4cf5b",
            "persistent image offer must carry its digest and nonce-keyed proof");
    Require(session.AcceptAsset(bundle, 1, "ready"), "persistent image can be acknowledged without a transfer");
    const auto waiting = session.Tick(bundle, 0.12f);
    Require(waiting.bytes[0] == 2 && waiting.bytes[1] == 2, "cached image must advance directly to the next offer");
    Require(session.Tick(bundle, 1.0f).size == 0, "unanswered offer must wait for its deadline");
    const auto resumed = session.Tick(bundle, 30.0f);
    Require(resumed.size > 0 && resumed.bytes[0] == 2 && resumed.bytes[1] == 3,
            "timed out asset must not stall the rest of the bundle");
    Require(!session.AcceptAsset(bundle, 2, "need"), "timed out asset cannot be requested after advancement");
    Require(session.AcceptAsset(bundle, 3, "reject") && session.Ready(bundle),
            "a rejected asset must not prevent other forms from becoming available");
}

void FormRequiresAssetsAndAVisibleToken() {
    Asset layout = MakeAsset("shop", AssetKind::Xaml,
        R"(<Window Width="640" Height="400" Title="Shop"><StackPanel>)"
        R"(<Button Content="Buy armor"/><Image Source="emblem"/></StackPanel></Window>)");
    Require(ParseForm(layout.bytes, layout.form), "shop fixture must be a valid form");
    const Bundle bundle{std::move(layout), MakeAsset("emblem", AssetKind::PersistentImage, "cached")};
    Session session;
    Negotiate(session, bundle);
    Require(!session.Show(bundle, "shop"), "unacknowledged layout must not show");
    Require(session.Tick(bundle, 0.06f).bytes[0] == 2 && session.AcceptAsset(bundle, 1, "need"),
            "layout transfer must begin after its offer");
    Require(session.Tick(bundle, 0.12f).bytes[0] == 3 && session.AcceptAsset(bundle, 1, "ready"),
            "layout transfer must complete before show");
    Require(!session.Show(bundle, "shop"), "a form with a pending image must not show");
    Require(session.Tick(bundle, 0.18f).bytes[1] == 2 && session.AcceptAsset(bundle, 2, "ready"),
            "referenced image must be available before the form can show");
    Require(session.Show(bundle, "shop"), "available form with available images must queue a show");
    Require(!session.Action(bundle, 1, 1, {}, 0.19f), "queued show must not accept actions before transmission");

    const auto show = session.Tick(bundle, 0.24f);
    Require(show.size == 6 && show.bytes[0] == 4 && show.bytes[1] == 1 && show.bytes[2] == 1 &&
            show.bytes[3] == 0 && show.bytes[4] == 0 && show.bytes[5] == 0,
            "show must carry its form ID and nonzero little-endian token");
    Require(!session.Action(bundle, 0, 1, {}, 0.25f) && !session.Action(bundle, 1, 2, {}, 0.25f) &&
            !session.Action(bundle, 1, 1, "unexpected", 0.25f),
            "stale tokens, noninteractive controls and invalid button values must be rejected");
    for (int action = 0; action < 12; ++action) {
        Require(session.Action(bundle, 1, 1, {}, 0.25f) == &bundle[0],
                "ordinary rapid actions on a visible form should consume the available burst");
    }
    Require(!session.Action(bundle, 1, 1, {}, 0.26f), "actions past the available burst must be rejected");
    Require(session.Action(bundle, 1, 1, {}, 0.31f), "action allowance must refill over time");
    Require(session.Hide() && !session.Action(bundle, 1, 1, {}, 0.42f), "queueing hide must invalidate actions");
    const auto hide = session.Tick(bundle, 0.42f);
    Require(hide.size == 5 && hide.bytes[0] == 5 && hide.bytes[1] == 1 && hide.bytes[2] == 0 &&
            hide.bytes[3] == 0 && hide.bytes[4] == 0, "hide must name the active token");

    Require(session.Show(bundle, "shop"), "closing a form must preserve availability of its assets");
    Require(session.Tick(bundle, 0.48f).bytes[2] == 2, "reopen must advance the form token");
    Require(session.Action(bundle, 2, 1, {}, 0.49f), "reopened form must receive valid actions");
    Require(session.Show(bundle, "shop") && session.Tick(bundle, 0.54f).bytes[2] == 3,
            "replacing an open form must advance the form token");
    for (int action = 0; action < 4; ++action) {
        Require(session.Action(bundle, 3, 1, {}, 0.56f), "replacement form must retain the replenished action allowance");
    }
    Require(!session.Action(bundle, 3, 1, {}, 0.56f), "replacing an open form must preserve the action rate limit");
    Require(!session.Action(bundle, 3, 0, {}, 0.57f) && !session.Hide(),
            "client dismissal must close the active form without invoking Pawn");
}

} // namespace

int main() {
    TransfersAreBoundedAndOrdered();
    PersistentImageCacheAndTimeout();
    FormRequiresAssetsAndAVisibleToken();
}
