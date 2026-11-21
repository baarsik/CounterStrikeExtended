#include "cse_custom_weapons.h"
#include "custom_weapon_ammo_layout.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

using namespace cse::weapons;

int main(int argc, char** argv) {
    const auto normalHud = AmmoHudLayout(1280, 720, 20, 12, 16, 16);
    assert(normalHud.primaryY == 690);
    assert(normalHud.secondaryY == 665);
    assert(normalHud.rightEdge == 1268);
    assert(MoneyHudY(660, nullptr) == 660);
    assert(MoneyHudY(660, &normalHud) == 635);

    const auto tallIcons = AmmoHudLayout(640, 480, 14, 8, 24, 32);
    assert(tallIcons.primaryY - tallIcons.secondaryY == 35);
    assert(AmmoDigits(0) == 1);
    assert(AmmoDigits(17) == 2);
    assert(AmmoDigits(1000) == 4);
    assert(AmmoDigits(65535) == 5);
    State original{};
    original.generation = 7;
    original.sequence = 101;
    original.subject = 2;
    original.active = 1001;
    original.animationSerial = 12;
    original.animation = 5;
    original.clip = 20;
    original.reserve = 180;
    original.secondary = 10;
    original.flags = SecondaryMode;
    original.count = 2;
    original.items[0] = {1001, 1};
    original.items[1] = {1002, 2};
    const Packet packet = Encode(original);
    assert(packet.size > 0 && packet.size <= MaxPacketBytes);
    State state{};
    assert(Decode(packet.bytes, packet.size, state));
    assert(state.secondary == 10 && state.clip == 20 && state.reserve == 180);
    assert(state.animationSerial == 12 && state.animation == 5);
    assert(state.items[1].instance == 1002 && state.items[1].definition == 2);
    for (int length = 0; length < packet.size; ++length) {
        assert(!Decode(packet.bytes, length, state));
    }
    original.items[1].instance = original.items[0].instance;
    assert(Encode(original).size == 0);
    original.items[1] = {1002, 2};
    original.active = 2000;
    assert(Encode(original).size == 0);

    Definition definition{};
    constexpr std::string_view source =
        "version=1\nkey=weapon_svdex\nlabel=SVD Custom\nview=models/test/view.mdl\n"
        "player=models/test/player.mdl\nworld=models/test/world.mdl\n"
        "primary=custom:svd_rifle\nprimary_capacity=180\nprimary_reserve=180\nclip=20\n"
        "secondary=custom:svd_aux\nsecondary_capacity=10\nsecondary_reserve=10\n"
        "projectile=models/test/shell.mdl\n";
    assert(ParseDefinition(source, definition) == ParseResult::Ok);
    assert(definition.clip == 20 && definition.primary.initialReserve == 180);
    assert(definition.secondary.initialReserve == 10 && definition.secondary.capacity == 10);
    assert(std::strcmp(definition.primary.key, definition.secondary.key) != 0);
    assert(definition.presentation.flags & HasSecondary);
    assert(ParseDefinition("version=1\nkey=weapon_svdex\nview=models/../hidden.mdl\n", definition) != ParseResult::Ok);
    assert(ParseDefinition("version=1\nversion=1\n", definition) != ParseResult::Ok);
    assert(ParseDefinition(std::string(source) + "primary_buy_amount=30\n", definition) ==
        ParseResult::InvalidValue);
    assert(ParseDefinition(std::string(source) + "primary_buy_amount=30\nprimary_buy_price=80\n", definition) ==
        ParseResult::Ok);
    assert(!IsAssetPath("models/test/../hidden.mdl", "models/", ".mdl"));
    assert(IsAssetPath("models/nimroK/weapons/v_svdex.mdl", "models/", ".mdl"));

    CatalogItem item{};
    item.generation = 7;
    item.weapon = definition.presentation;
    item.weapon.id = 2;
    const Packet catalog = Encode(item);
    CatalogItem decoded{};
    assert(Decode(catalog.bytes, catalog.size, decoded));
    assert(std::strcmp(decoded.weapon.key, "weapon_svdex") == 0);
    assert(UpdateDigest(InitialDigest, item) == UpdateDigest(InitialDigest, decoded));
    decoded.generation = 8;
    assert(UpdateDigest(InitialDigest, item) != UpdateDigest(InitialDigest, decoded));

    assert(argc == 2);
    std::ifstream fixture(argv[1], std::ios::binary);
    const std::string fixtureContents{std::istreambuf_iterator<char>(fixture), std::istreambuf_iterator<char>()};
    PrimaryAmmoPurchase purchase{};
    assert(ParseDefinition(fixtureContents, definition, nullptr, &purchase) == ParseResult::Ok);
    assert(definition.automatic == 1 && definition.clip == 20 && definition.primary.initialReserve == 180);
    assert(purchase.amount == 30 && purchase.price == 80);
    assert(definition.secondary.initialReserve == 10 && definition.secondaryFireAnim == 5);
    assert(std::strcmp(definition.animationSounds[7], "weapons/svdex_foley4.wav") == 0);
}
