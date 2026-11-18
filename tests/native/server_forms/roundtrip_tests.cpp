#include "../../../src/cstrike/cl_dll/server_form_session.h"
#include "../../../src/amxx/csextended/forms/session.h"
#include "image_codec.h"

#include <cassert>
#include <chrono>

namespace forms = cse::forms;
namespace server = cse::forms::server;

unsigned Connect(forms::Session& client, server::Session& peer, const server::Bundle& bundle,
    const std::filesystem::path& root, std::string_view address, std::uint8_t challenge, bool legacy = false) {
    forms::Nonce nonce{};
    nonce[0] = challenge;
    client.Begin(nonce, root, address);
    peer.Begin();
    unsigned chunks = 0;
    for (unsigned tick = 0; tick < 400 && !peer.Ready(bundle); ++tick) {
        const auto message = peer.Tick(bundle, static_cast<float>(tick) * 0.06f);
        if (!message.size) {
            continue;
        }
        const auto op = static_cast<forms::Op>(message.bytes[0]);
        if (op == forms::Op::Hello) {
            assert(peer.AcceptHello(forms::EncodeHex(nonce.data(), nonce.size())));
            continue;
        }
        if (op == forms::Op::Chunk) {
            ++chunks;
        }
        if (legacy && op == forms::Op::Offer && forms::IsWebPImage(static_cast<forms::AssetKind>(message.bytes[2]))) {
            assert(peer.AcceptAsset(bundle, message.bytes[1], "reject"));
            continue;
        }
        const auto reply = client.Receive({reinterpret_cast<const char*>(message.bytes.data()), message.size});
        if (reply.status == forms::AssetStatus::Need) {
            assert(peer.AcceptAsset(bundle, reply.id, "need"));
        } else if (reply.status == forms::AssetStatus::Ready) {
            assert(peer.AcceptAsset(bundle, reply.id, "ready"));
        } else {
            assert(reply.status == forms::AssetStatus::Ignored && op == forms::Op::Chunk);
        }
    }
    assert(peer.Ready(bundle));
    if (legacy) {
        assert(!client.FindForm(1) && !peer.Show(bundle, "shop"));
        assert(client.FindImage("temp"));
        return chunks;
    }
    assert(client.FindForm(1) && peer.Show(bundle, "shop"));
    const auto open = peer.Tick(bundle, 30.0f);
    assert(open.size == 6 && open.bytes[0] == static_cast<std::uint8_t>(forms::Op::Show));
    const std::uint32_t token = open.bytes[2] | (open.bytes[3] << 8) |
        (std::uint32_t{open.bytes[4]} << 16) | (std::uint32_t{open.bytes[5]} << 24);
    assert(peer.Action(bundle, token, 1, "", 31.0f) == &bundle.front());
    assert(!peer.Action(bundle, token - 1, 1, "", 32.0f));
    return chunks;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::path(argv[1]) / ("roundtrip-test-" + std::to_string(run));
    const std::string layout("form 640 400 Shop\nbutton 10 10 100 24 A 0 0 Buy\n"
        "image 10 50 32 32 - 0 0 logo\nimage 50 50 32 32 - 0 0 temp\nimage 90 50 32 32 - 0 0 session\n");
    forms::Form form;
    assert(forms::ParseForm(layout, form));
    const std::string pixels("\1\0\1\0\xff\xff\0\xff", 8);
    std::vector<unsigned char> encoded;
    const unsigned char webpPixels[]{255, 255, 0, 255};
    assert(cse::image::EncodeWebPLossless(webpPixels, 1, 1, 4, 4, false, encoded));
    const std::string webp(encoded.begin(), encoded.end());
    forms::Image decoded;
    assert(!forms::ParseImage(forms::AssetKind::WebPImage, pixels, decoded));
    assert(!forms::ParseImage(forms::AssetKind::Image, webp, decoded));
    assert(!forms::ParseImage(forms::AssetKind::WebPImage, std::string_view(webp).substr(0, webp.size() - 1), decoded));
    const std::vector<unsigned char> tallPixels((forms::kMaxImageSide + 1) * 4, 255);
    assert(cse::image::EncodeWebPLossless(tallPixels.data(), 1, forms::kMaxImageSide + 1, 4, 4, false, encoded));
    assert(!forms::ParseImage(forms::AssetKind::WebPImage,
        {reinterpret_cast<const char*>(encoded.data()), encoded.size()}, decoded));
    const server::Bundle bundle{
        {"shop", forms::AssetKind::Form, layout, forms::Hash(layout), form},
        {"logo", forms::AssetKind::PersistentWebPImage, webp, forms::Hash(webp), {}},
        {"temp", forms::AssetKind::Image, pixels, forms::Hash(pixels), {}},
        {"session", forms::AssetKind::WebPImage, webp, forms::Hash(webp), {}},
    };
    forms::Session client;
    server::Session peer;
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 1) == 4);
    assert(client.FindImage("logo")->pixels == std::vector<std::uint8_t>(webpPixels, webpPixels + 4));
    assert(client.FindImage("session")->pixels == client.FindImage("logo")->pixels);
    client.Reset();
    assert(!client.FindImage("temp") && !client.FindImage("session") && !client.FindForm(1));
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 2) == 3);
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27016", 3) == 4);
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 4, true) == 2);
    std::filesystem::remove_all(root);
}
