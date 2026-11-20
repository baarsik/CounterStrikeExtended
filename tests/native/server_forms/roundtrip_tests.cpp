#include "../../../src/cstrike/cl_dll/server_form_session.h"
#include "../../../src/amxx/csextended/forms/session.h"
#include "image_codec.h"

#include <cassert>
#include <chrono>

namespace forms = cse::forms;
namespace server = cse::forms::server;

void RecoverTimedOutOffer(const std::filesystem::path& root) {
    const std::string layout = R"(<Window Width="400" Height="250" Title="Shop"><Button Content="Buy"/></Window>)";
    forms::Form form;
    assert(forms::ParseForm(layout, form));
    const server::Bundle bundle{
        {"missed", forms::AssetKind::Xaml, layout, forms::Hash(layout), form},
        {"shop", forms::AssetKind::Xaml, layout, forms::Hash(layout), form},
    };
    forms::Session client;
    server::Session peer;
    const forms::Nonce nonce{};
    client.Begin(nonce, root, "127.0.0.1:27015");
    peer.Begin();
    const auto receive = [&client](const server::Message& message) {
        return client.Receive({reinterpret_cast<const char*>(message.bytes.data()), message.size});
    };

    assert(peer.Tick(bundle, 0.0f).bytes[0] == static_cast<std::uint8_t>(forms::Op::Hello));
    assert(peer.AcceptHello(forms::EncodeHex(nonce.data(), nonce.size())));
    const auto missed = peer.Tick(bundle, 0.06f);
    constexpr float afterOfferTimeout = 9.0f;
    const auto next = peer.Tick(bundle, afterOfferTimeout);
    const auto late = receive(missed);
    assert(late.id == 1 && late.status == forms::AssetStatus::Need);
    assert(!peer.AcceptAsset(bundle, late.id, "need"));

    const auto requested = receive(next);
    assert(requested.id == 2 && requested.status == forms::AssetStatus::Need);
    assert(peer.AcceptAsset(bundle, requested.id, "need"));
    const auto finished = receive(peer.Tick(bundle, afterOfferTimeout + 0.06f));
    assert(finished.id == 2 && finished.status == forms::AssetStatus::Ready);
    assert(peer.AcceptAsset(bundle, finished.id, "ready"));
    assert(!client.FindForm(1) && client.FindForm(2));
    assert(peer.Ready(bundle) && peer.Show(bundle, "shop"));
}

void MenuOverrides(forms::Session& client, server::Session& peer, const server::Bundle& bundle, bool legacy) {
    forms::MenuBindings bindings{};
    bindings[forms::MenuIndex(forms::StockMenu::Buy)] = 1;
    const auto announcement = peer.Tick(bundle, 28.0f, bindings);
    if (legacy) {
        assert(!announcement.size);
        assert(!peer.ShowMenu(bundle, forms::StockMenu::Buy, 1, 29.0f));
        return;
    }

    assert(announcement.size == 1 + bindings.size());
    const std::string_view packet(reinterpret_cast<const char*>(announcement.bytes.data()), announcement.size);
    assert(!client.ReceiveMenuBindings(packet.substr(0, packet.size() - 1)));
    assert(client.ReceiveMenuBindings(packet));
    assert(client.MenuForm(forms::StockMenu::Buy) == 1);
    assert(client.MenuForm(forms::StockMenu::Team) == 0);
    assert(!peer.ShowMenu(bundle, forms::StockMenu::Team, 77, 29.0f));
    assert(!peer.ShowMenu(bundle, forms::StockMenu::Buy, 0, 29.0f));
    assert(peer.ShowMenu(bundle, forms::StockMenu::Buy, 77, 29.0f));
    const auto show = peer.Tick(bundle, 29.0f, bindings);
    assert(show.size == 10 && show.bytes[0] == static_cast<std::uint8_t>(forms::Op::MenuShow) &&
        show.bytes[1] == 1 && show.bytes[6] == 77);
    assert(!peer.ShowMenu(bundle, forms::StockMenu::Buy, 78, 29.01f));
}

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
    MenuOverrides(client, peer, bundle, legacy);
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
    assert(forms::VguiStockMenu(2) == forms::StockMenu::Team);
    assert(forms::VguiStockMenu(26) == forms::StockMenu::TerroristClass);
    assert(forms::VguiStockMenu(27) == forms::StockMenu::CounterTerroristClass);
    assert(forms::VguiStockMenu(28) == forms::StockMenu::Buy);
    assert(forms::VguiStockMenu(34) == forms::StockMenu::BuyEquipment);
    assert(forms::VguiStockMenu(3) == forms::StockMenu::None);
    assert(forms::TextStockMenu("#IG_VIP_Team_Select_Spect") == forms::StockMenu::Team);
    assert(forms::TextStockMenu("#Terrorist_Select") == forms::StockMenu::TerroristClass);
    assert(forms::TextStockMenu("#CT_Select") == forms::StockMenu::CounterTerroristClass);
    assert(forms::TextStockMenu("#Buy") == forms::StockMenu::Buy);
    assert(forms::TextStockMenu("#DCT_BuyItem") == forms::StockMenu::BuyEquipment);
    assert(forms::TextStockMenu("#BuySomething") == forms::StockMenu::None);
    assert(forms::TextStockMenu("#Buy\n1. Plugin option") == forms::StockMenu::None);
    const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::path(argv[1]) / ("roundtrip-test-" + std::to_string(run));
    RecoverTimedOutOffer(root);
    const std::string layout = R"(<Window Width="640" Height="400" Title="Shop"><StackPanel>)"
        R"(<Button Content="Buy"/><Image Source="logo"/><Image Source="temp"/><Image Source="session"/>)"
        R"(</StackPanel></Window>)";
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
        {"shop", forms::AssetKind::Xaml, layout, forms::Hash(layout), form},
        {"logo", forms::AssetKind::PersistentWebPImage, webp, forms::Hash(webp), {}},
        {"temp", forms::AssetKind::Image, pixels, forms::Hash(pixels), {}},
        {"session", forms::AssetKind::WebPImage, webp, forms::Hash(webp), {}},
    };
    const auto layoutChunks = (layout.size() + forms::kMaxChunkBytes - 1) / forms::kMaxChunkBytes;
    forms::Session client;
    server::Session peer;
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 1) == layoutChunks + 3);
    assert(client.FindImage("logo")->pixels == std::vector<std::uint8_t>(webpPixels, webpPixels + 4));
    assert(client.FindImage("session")->pixels == client.FindImage("logo")->pixels);
    client.Reset();
    assert(!client.FindImage("temp") && !client.FindImage("session") && !client.FindForm(1));
    assert(!client.MenuForm(forms::StockMenu::Buy));
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 2) == layoutChunks + 2);
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27016", 3) == layoutChunks + 3);
    assert(Connect(client, peer, bundle, root, "127.0.0.1:27015", 4, true) == layoutChunks + 1);
    std::filesystem::remove_all(root);
}
