#include "../../../src/cstrike/cl_dll/server_form_session.h"

#include <cassert>
#include <string>

using namespace cse::forms;

std::string Offer(unsigned id, AssetKind kind, std::string_view bytes, std::string_view name, const Nonce& nonce) {
    std::string packet{static_cast<char>(Op::Offer), static_cast<char>(id), static_cast<char>(kind),
        static_cast<char>(bytes.size()), static_cast<char>(bytes.size() >> 8)};
    const auto digest = Hash(bytes);
    const auto proof = kind == AssetKind::PersistentImage ? MakeProof(nonce, bytes) : Digest{};
    packet.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    packet.append(reinterpret_cast<const char*>(proof.data()), proof.size());
    packet.append(name);
    return packet;
}

std::string Chunk(unsigned id, unsigned offset, std::string_view bytes) {
    std::string packet{static_cast<char>(Op::Chunk), static_cast<char>(id), static_cast<char>(offset),
        static_cast<char>(offset >> 8)};
    packet.append(bytes);
    return packet;
}

int main() {
    using Time = Presentation::Time;
    using namespace std::chrono_literals;
    Presentation ui;
    const Time start{};
    assert(ui.Queue({1, 1}, start));
    assert(ui.TakeReady(start).token == 1);
    assert(ui.Queue({2, 2}, start + 50ms));
    assert(!ui.TakeReady(start + 100ms).token);
    assert(ui.Queue({3, 3}, start + 150ms));
    assert(ui.TakeReady(start + 250ms).token == 3);
    assert(!ui.Queue({2, 2}, start + 300ms));
    ui.Dismiss(start + 400ms);
    assert(ui.Queue({4, 1}, start + 450ms));
    assert(!ui.TakeReady(start + 1300ms).token);
    assert(ui.Queue({5, 1}, start + 1300ms));
    ui.Cancel(5);
    assert(!ui.TakeReady(start + 1300ms).token);
    ui.Dismiss(start + 1400ms);
    assert(ui.Queue({6, 1, 42}, start + 1500ms));
    assert(ui.TakeReady(start + 1500ms).menuRequest == 42);

    Session session;
    Nonce nonce{};
    const std::string_view menu = "form 640 400 Shop\nbutton 10 10 100 24 1 0 0 Buy\nimage 10 50 32 32 - 0 0 icon\n";
    const std::string image("\1\0\1\0\xff\xff\0\xff", 8);
    assert(session.Receive(Offer(1, AssetKind::Form, menu, "shop", nonce)).status == AssetStatus::Ignored);
    session.Begin(nonce, {}, "127.0.0.1:27015");
    assert(session.Receive(Offer(1, AssetKind::Form, menu, "shop", nonce)).status == AssetStatus::Need);
    assert(!session.FindForm(1));
    assert(session.Receive(Offer(2, AssetKind::Image, image, "icon", nonce)).status == AssetStatus::Ignored);
    assert(session.Receive(Chunk(1, 0, menu.substr(0, 24))).status == AssetStatus::Ignored);
    assert(session.Receive(Chunk(1, 24, menu.substr(24))).status == AssetStatus::Ready);
    assert(!session.FindForm(1));
    assert(session.Receive(Offer(2, AssetKind::Image, image, "icon", nonce)).status == AssetStatus::Need);
    assert(session.Receive(Chunk(2, 0, image)).status == AssetStatus::Ready);
    assert(session.FindForm(1) && session.FindImage("icon"));
    const std::string stockMenus{static_cast<char>(Op::MenuBindings), 1, 2, 0, 33, 0};
    assert(session.ReceiveMenuBindings(stockMenus));
    assert(session.MenuForm(StockMenu::Team) == 1);
    assert(!session.MenuForm(StockMenu::TerroristClass) && !session.MenuForm(StockMenu::Buy));
    assert(session.Receive(Offer(3, AssetKind::Form, menu, "shop", nonce)).status == AssetStatus::Reject);
    assert(session.Receive(Offer(4, AssetKind::Form, menu, "../config", nonce)).status == AssetStatus::Reject);
    assert(session.Receive(Offer(5, AssetKind::Form, menu, "fresh", nonce)).status == AssetStatus::Need);
    assert(session.Receive(Chunk(5, 1, menu)).status == AssetStatus::Reject);
    assert(!session.FindForm(5));
    assert(session.Receive(Offer(6, AssetKind::Form, menu, "corrupt", nonce)).status == AssetStatus::Need);
    auto altered = std::string(menu);
    altered[15] = 'B';
    assert(session.Receive(Chunk(6, 0, altered)).status == AssetStatus::Reject);
    assert(!session.FindForm(6));
    session.Reset();
    assert(!session.Active() && !session.FindForm(1) && !session.FindImage("icon"));
    assert(session.Receive(Chunk(2, 0, image)).status == AssetStatus::Ignored);
    session.Begin(nonce, {}, "127.0.0.1:27016");
    assert(!session.FindForm(1) && !session.FindImage("icon"));
    assert(session.Receive(Chunk(2, 0, image)).status == AssetStatus::Ignored);
}
