#include "cse_custom_weapons.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <type_traits>

namespace cse::weapons {
namespace {

using std::string_view;

constexpr bool Letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

constexpr bool Lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

constexpr bool Digit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool IsKey(string_view key) {
    return !key.empty() && Lower(key.front()) && std::all_of(key.begin(), key.end(), [](char ch) {
        return Lower(ch) || Digit(ch) || ch == '_';
    });
}

bool IsLabel(string_view label) {
    if (label.empty()) {
        return false;
    }
    const auto* cursor = reinterpret_cast<const unsigned char*>(label.data());
    const auto* end = cursor + label.size();
    while (cursor < end) {
        const unsigned char lead = *cursor++;
        if (lead < 0x20 || lead == 0x7f) {
            return false;
        }
        if (lead < 0x80) {
            continue;
        }
        unsigned bytes;
        uint32_t value;
        uint32_t minimum;
        if (lead >= 0xc2 && lead <= 0xdf) {
            bytes = 1;
            value = lead & 0x1f;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            bytes = 2;
            value = lead & 0x0f;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            bytes = 3;
            value = lead & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (end - cursor < bytes) {
            return false;
        }
        for (unsigned i = 0; i < bytes; ++i) {
            const unsigned char continuation = *cursor++;
            if ((continuation & 0xc0) != 0x80) {
                return false;
            }
            value = (value << 6) | (continuation & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

template<std::size_t N>
string_view View(const char (&value)[N]) {
    const auto* end = static_cast<const char*>(std::memchr(value, '\0', N));
    return string_view(value, end ? static_cast<std::size_t>(end - value) : N);
}

template<std::size_t N>
bool Copy(char (&out)[N], string_view source) {
    if (source.size() >= N) {
        return false;
    }
    if (!source.empty()) {
        std::memcpy(out, source.data(), source.size());
    }
    out[source.size()] = '\0';
    return true;
}

string_view Trim(string_view value) {
    const auto begin = value.find_first_not_of(" \t\r");
    if (begin == string_view::npos) {
        return {};
    }
    return value.substr(begin, value.find_last_not_of(" \t\r") - begin + 1);
}

template<typename T>
bool Number(string_view text, T& result, unsigned maximum = std::numeric_limits<T>::max()) {
    unsigned number = 0;
    if (text.empty()) {
        return false;
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number > maximum) {
        return false;
    }
    result = static_cast<T>(number);
    return true;
}

template<typename T>
struct Field {
    string_view key;
    T Definition::*member;
    unsigned maximum;
};

constexpr Field<uint16_t> Counts[] = {
    {"clip", &Definition::clip, 65535}, {"initial_clip", &Definition::initialClip, 65535},
    {"price", &Definition::price, 65535}, {"damage", &Definition::damage, 10000},
    {"secondary_damage", &Definition::secondaryDamage, 10000},
    {"secondary_radius", &Definition::secondaryRadius, 2048}, {"projectile_speed", &Definition::projectileSpeed, 4096},
    {"fire_ms", &Definition::fireIntervalMs, 30000}, {"secondary_ms", &Definition::secondaryIntervalMs, 30000},
    {"switch_ms", &Definition::switchIntervalMs, 30000}, {"reload_ms", &Definition::reloadMs, 30000},
    {"deploy_ms", &Definition::deployMs, 30000}, {"range", &Definition::range, 16384},
    {"speed", &Definition::speed, 400}, {"spread", &Definition::spreadThousandths, 1000},
    {"recoil", &Definition::recoilThousandths, 20000},
};

constexpr Field<uint8_t> Bytes[] = {
    {"idle_anim", &Definition::idleAnim, 255}, {"fire_anim", &Definition::fireAnim, 255},
    {"reload_anim", &Definition::reloadAnim, 255}, {"draw_anim", &Definition::drawAnim, 255},
    {"secondary_idle_anim", &Definition::secondaryIdleAnim, 255},
    {"secondary_fire_anim", &Definition::secondaryFireAnim, 255},
    {"secondary_empty_anim", &Definition::secondaryEmptyAnim, 255},
    {"secondary_draw_anim", &Definition::secondaryDrawAnim, 255},
    {"switch_to_secondary_anim", &Definition::switchToSecondaryAnim, 255},
    {"switch_to_primary_anim", &Definition::switchToPrimaryAnim, 255},
    {"automatic", &Definition::automatic, 1}, {"droppable", &Definition::droppable, 1},
    {"zoom", &Definition::zoom, 89}, {"team", &Definition::team, 2},
    {"respawn", &Definition::respawn, 1}, {"weight", &Definition::weight, 255},
};

constexpr Field<char[64]> Assets[] = {
    {"view", &Definition::viewModel, 0}, {"player", &Definition::playerModel, 0},
    {"world", &Definition::worldModel, 0}, {"projectile", &Definition::projectileModel, 0},
    {"fire_sound", &Definition::fireSound, 1}, {"secondary_sound", &Definition::secondarySound, 1},
    {"impact_sound", &Definition::impactSound, 1},
};

bool AmmoKey(string_view text, Ammo& ammo) {
    constexpr string_view Custom = "custom:";
    constexpr string_view Stock = "stock:";
    if (text.substr(0, Stock.size()) == Stock) {
        ammo.stock = 1;
        text.remove_prefix(Stock.size());
    } else if (text.substr(0, Custom.size()) == Custom) {
        text.remove_prefix(Custom.size());
    } else {
        return false;
    }
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char ch) {
        return Lower(ch) || Digit(ch) || ch == '_';
    }) && Copy(ammo.key, text);
}

ParseResult SetField(Definition& d, PrimaryAmmoPurchase& purchase, string_view key, string_view value) {
    for (const auto& field : Counts) {
        if (field.key == key) {
            return Number(value, d.*(field.member), field.maximum) ? ParseResult::Ok : ParseResult::InvalidValue;
        }
    }
    for (const auto& field : Bytes) {
        if (field.key == key) {
            return Number(value, d.*(field.member), field.maximum) ? ParseResult::Ok : ParseResult::InvalidValue;
        }
    }
    for (const auto& field : Assets) {
        if (field.key == key) {
            const bool sound = field.maximum != 0;
            return IsAssetPath(value, sound ? "weapons/" : "models/", sound ? ".wav" : ".mdl") &&
                Copy(d.*(field.member), value) ? ParseResult::Ok : ParseResult::InvalidValue;
        }
    }
    bool valid;
    if (key == "version") {
        return value == "1" ? ParseResult::Ok : ParseResult::Incompatible;
    }
    if (key == "key") {
        valid = IsWeaponKey(value) && Copy(d.presentation.key, value);
    } else if (key == "label") {
        valid = IsLabel(value) && Copy(d.presentation.label, value);
    } else if (key == "hud") {
        valid = IsAssetPath(value, "sprites/", ".txt") && Copy(d.presentation.hud, value);
    } else if (key == "slot") {
        valid = Number(value, d.presentation.slot, 1);
    } else if (key == "order") {
        valid = Number(value, d.presentation.order);
    } else if (key == "primary") {
        valid = AmmoKey(value, d.primary);
    } else if (key == "secondary") {
        valid = AmmoKey(value, d.secondary);
    } else if (key == "primary_capacity") {
        valid = Number(value, d.primary.capacity);
    } else if (key == "primary_reserve") {
        valid = Number(value, d.primary.initialReserve);
    } else if (key == "primary_buy_amount") {
        valid = Number(value, purchase.amount) && purchase.amount > 0;
    } else if (key == "primary_buy_price") {
        valid = Number(value, purchase.price) && purchase.price > 0;
    } else if (key == "secondary_capacity") {
        valid = Number(value, d.secondary.capacity);
    } else if (key == "secondary_reserve") {
        valid = Number(value, d.secondary.initialReserve);
    } else if (constexpr string_view Prefix = "animation_sound_"; key.substr(0, Prefix.size()) == Prefix) {
        unsigned index = 0;
        valid = Number(key.substr(Prefix.size()), index, MaxAnimationSounds) && index > 0 &&
            IsAssetPath(value, "weapons/", ".wav") && Copy(d.animationSounds[index - 1], value);
    } else {
        return ParseResult::UnknownField;
    }
    return valid ? ParseResult::Ok : ParseResult::InvalidValue;
}

class Writer {
public:
    explicit Writer(Message type) {
        Put(ProtocolVersion);
        Put(static_cast<uint8_t>(type));
    }

    template<typename T>
    void Put(T value) {
        static_assert(std::is_unsigned_v<T>);
        if (packet_.size + static_cast<int>(sizeof(T)) > MaxPacketBytes) {
            valid_ = false;
            return;
        }
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            packet_.bytes[packet_.size++] = static_cast<uint8_t>(value >> (i * 8));
        }
    }

    template<std::size_t N>
    void Text(const char (&text)[N]) {
        const auto value = View(text);
        if (value.size() == N) {
            valid_ = false;
            return;
        }
        Put(static_cast<uint8_t>(value.size()));
        for (const char ch : value) {
            Put(static_cast<uint8_t>(ch));
        }
    }

    Packet Finish() const {
        return valid_ ? packet_ : Packet{};
    }

private:
    Packet packet_{};
    bool valid_ = true;
};

class Reader {
public:
    Reader(const void* data, int length, Message type)
        : bytes_(static_cast<const unsigned char*>(data)), length_(length), valid_(Identify(data, length) == type) {}

    template<typename T>
    T Get() {
        if (!valid_ || length_ - offset_ < static_cast<int>(sizeof(T))) {
            valid_ = false;
            return 0;
        }
        uint32_t value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<uint32_t>(bytes_[offset_++]) << (i * 8);
        }
        return static_cast<T>(value);
    }

    template<std::size_t N>
    void Text(char (&text)[N]) {
        const uint8_t size = Get<uint8_t>();
        if (!valid_ || length_ - offset_ < size || size >= N || std::memchr(bytes_ + offset_, '\0', size)) {
            valid_ = false;
            return;
        }
        Copy(text, string_view(reinterpret_cast<const char*>(bytes_ + offset_), size));
        offset_ += size;
    }

    bool Complete() const { return valid_ && offset_ == length_; }

private:
    const unsigned char* bytes_;
    int length_;
    int offset_ = 2;
    bool valid_;
};

bool Valid(const Presentation& p) {
    const auto hud = View(p.hud);
    return p.id > 0 && p.id <= MaxDefinitions && IsWeaponKey(View(p.key)) && IsLabel(View(p.label)) &&
        (hud.empty() || IsAssetPath(hud, "sprites/", ".txt")) && p.slot < MaxHudSlots &&
        (p.flags & ~(HasSecondary | Scope)) == 0;
}

bool Valid(const State& state) {
    if (state.generation == 0 || state.sequence == 0 || state.count > MaxOwned || state.subject > MaxPlayers ||
        (state.flags & ~(SecondaryMode | NoClip | Observing | Reloading)) != 0 || state.zoom > 90 ||
        state.pickup > MaxDefinitions || ((state.flags & Observing) && (state.subject == 0 || state.count > 1))) {
        return false;
    }
    bool active = state.active == 0;
    bool last = state.last == 0;
    for (unsigned i = 0; i < state.count; ++i) {
        const auto item = state.items[i];
        if (item.instance == 0 || item.definition == 0 || item.definition > MaxDefinitions) {
            return false;
        }
        for (unsigned earlier = 0; earlier < i; ++earlier) {
            if (state.items[earlier].instance == item.instance || state.items[earlier].definition == item.definition) {
                return false;
            }
        }
        active |= state.active == item.instance;
        last |= state.last == item.instance;
    }
    return active && last;
}

bool Valid(const Kill& kill) {
    return kill.generation > 0 && kill.sequence > 0 && kill.definition > 0 && kill.definition <= MaxDefinitions &&
        kill.killer <= MaxPlayers && kill.victim > 0 && kill.victim <= MaxPlayers && kill.headshot <= 1;
}

} // namespace

bool IsAssetPath(std::string_view path, std::string_view root, std::string_view extension) {
    constexpr std::size_t AssetPathBytes = 63;
    if (path.size() <= root.size() + extension.size() || path.size() > AssetPathBytes ||
        path.substr(0, root.size()) != root || path.substr(path.size() - extension.size()) != extension) {
        return false;
    }
    while (!path.empty()) {
        const auto separator = path.find('/');
        const auto part = path.substr(0, separator);
        if (part.empty() || !std::all_of(part.begin(), part.end(), [part, separator](char ch) {
                return Letter(ch) || Digit(ch) || ch == '_' || ch == '-' ||
                    (ch == '.' && separator == string_view::npos && part.front() != '.' &&
                        part.find('.') == part.rfind('.'));
            })) {
            return false;
        }
        if (separator == string_view::npos) {
            return true;
        }
        path.remove_prefix(separator + 1);
    }
    return false;
}

bool IsWeaponKey(std::string_view key) {
    constexpr std::string_view Prefix = "weapon_";
    return key.size() < sizeof(Presentation::key) && key.substr(0, Prefix.size()) == Prefix &&
        IsKey(key.substr(Prefix.size()));
}

std::string HudSpritePath(std::string_view asset) {
    constexpr std::string_view root = "sprites/";
    constexpr std::string_view legacyExtension = ".spr";
    std::string path(root);
    path.append(asset);
    if (asset.find('.') == std::string_view::npos) {
        path.append(legacyExtension);
    }
    if (IsAssetPath(path, root, legacyExtension) || IsAssetPath(path, root, ".webp")) {
        return path;
    }
    return {};
}

ParseResult ParseDefinition(std::string_view text, Definition& out, unsigned* errorLine,
    PrimaryAmmoPurchase* primaryAmmoPurchase) {
    constexpr std::size_t MaxDefinitionBytes = 16 * 1024;
    constexpr std::size_t MaxDefinitionFields = 64;
    if (errorLine) {
        *errorLine = 0;
    }
    if (text.size() > MaxDefinitionBytes) {
        return ParseResult::InvalidLine;
    }
    Definition d{};
    PrimaryAmmoPurchase purchase{};
    d.damage = 35;
    d.secondaryDamage = 85;
    d.secondaryRadius = 120;
    d.projectileSpeed = 650;
    d.fireIntervalMs = 250;
    d.secondaryIntervalMs = 1200;
    d.switchIntervalMs = 350;
    d.reloadMs = 2200;
    d.deployMs = 700;
    d.range = 8192;
    d.speed = 220;
    d.spreadThousandths = 12;
    d.recoilThousandths = 900;
    d.droppable = 1;
    d.weight = 25;
    std::array<string_view, MaxDefinitionFields> keys{};
    unsigned count = 0;
    unsigned lineNumber = 0;
    while (!text.empty()) {
        ++lineNumber;
        const auto newline = text.find('\n');
        const auto line = Trim(text.substr(0, newline));
        text.remove_prefix(newline == string_view::npos ? text.size() : newline + 1);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (errorLine) {
            *errorLine = lineNumber;
        }
        const auto delimiter = line.find('=');
        if (delimiter == string_view::npos || count == keys.size()) {
            return ParseResult::InvalidLine;
        }
        const auto key = Trim(line.substr(0, delimiter));
        if (std::find(keys.begin(), keys.begin() + count, key) != keys.begin() + count) {
            return ParseResult::DuplicateField;
        }
        const auto result = SetField(d, purchase, key, Trim(line.substr(delimiter + 1)));
        if (result != ParseResult::Ok) {
            return result;
        }
        keys[count++] = key;
    }
    if (errorLine) {
        *errorLine = 0;
    }
    const auto seen = [&keys, count](string_view key) {
        return std::find(keys.begin(), keys.begin() + count, key) != keys.begin() + count;
    };
    if (!seen("version") || !d.presentation.key[0] || !d.presentation.label[0] || !d.viewModel[0] ||
        !d.playerModel[0] || !d.worldModel[0] || !d.primary.key[0] || !seen("clip")) {
        return ParseResult::MissingField;
    }
    if (!seen("initial_clip")) {
        d.initialClip = d.clip;
    }
    if (d.initialClip > d.clip || !d.damage || !d.fireIntervalMs || !d.reloadMs || !d.range || !d.speed ||
        ((purchase.amount == 0) != (purchase.price == 0)) ||
        (!d.primary.stock && (!d.primary.capacity || d.primary.initialReserve > d.primary.capacity))) {
        return ParseResult::InvalidValue;
    }
    if (d.secondary.key[0]) {
        if (!d.projectileModel[0] || !d.secondaryDamage || !d.secondaryRadius || !d.projectileSpeed ||
            !d.secondaryIntervalMs || !d.switchIntervalMs || d.zoom || d.secondary.stock || !d.secondary.capacity ||
            d.secondary.initialReserve > d.secondary.capacity ||
            (!d.primary.stock && View(d.secondary.key) == View(d.primary.key))) {
            return ParseResult::InvalidValue;
        }
        d.presentation.flags |= HasSecondary;
    } else if (d.projectileModel[0] || d.secondary.capacity || d.secondary.initialReserve) {
        return ParseResult::InvalidValue;
    }
    if (d.zoom) {
        d.presentation.flags |= Scope;
    }
    out = d;
    if (primaryAmmoPurchase) {
        *primaryAmmoPurchase = purchase;
    }
    return ParseResult::Ok;
}

Message Identify(const void* data, int length) {
    if (!data || length < 2 || length > MaxPacketBytes) {
        return {};
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    if (bytes[0] != ProtocolVersion || bytes[1] < static_cast<uint8_t>(Message::Begin) ||
        bytes[1] > static_cast<uint8_t>(Message::Kill)) {
        return {};
    }
    return static_cast<Message>(bytes[1]);
}

Packet Encode(const Begin& m) {
    Writer w(Message::Begin);
    w.Put(m.generation);
    w.Put(m.count);
    return m.generation && m.count <= MaxDefinitions ? w.Finish() : Packet{};
}

Packet Encode(const CatalogItem& m) {
    Writer w(Message::Definition);
    w.Put(m.generation);
    w.Put(m.weapon.id);
    w.Put(m.weapon.slot);
    w.Put(m.weapon.order);
    w.Put(m.weapon.flags);
    w.Text(m.weapon.key);
    w.Text(m.weapon.label);
    w.Text(m.weapon.hud);
    return m.generation && Valid(m.weapon) ? w.Finish() : Packet{};
}

Packet Encode(const End& m) {
    Writer w(Message::End);
    w.Put(m.generation);
    w.Put(m.digest);
    return m.generation ? w.Finish() : Packet{};
}

Packet Encode(const State& m) {
    if (!Valid(m)) {
        return {};
    }
    Writer w(Message::State);
    w.Put(m.generation);
    w.Put(m.sequence);
    w.Put(m.active);
    w.Put(m.last);
    w.Put(m.animationSerial);
    w.Put(m.clip);
    w.Put(m.reserve);
    w.Put(m.secondary);
    w.Put(m.pickup);
    w.Put(m.subject);
    w.Put(m.flags);
    w.Put(m.zoom);
    w.Put(m.animation);
    w.Put(m.count);
    for (unsigned i = 0; i < m.count; ++i) {
        w.Put(m.items[i].instance);
        w.Put(m.items[i].definition);
    }
    return w.Finish();
}

Packet Encode(const Kill& m) {
    Writer w(Message::Kill);
    w.Put(m.generation);
    w.Put(m.sequence);
    w.Put(m.definition);
    w.Put(m.killer);
    w.Put(m.victim);
    w.Put(m.headshot);
    return Valid(m) ? w.Finish() : Packet{};
}

bool Decode(const void* data, int length, Begin& m) {
    Reader r(data, length, Message::Begin);
    m.generation = r.Get<uint32_t>();
    m.count = r.Get<uint16_t>();
    return r.Complete() && m.generation && m.count <= MaxDefinitions;
}

bool Decode(const void* data, int length, CatalogItem& m) {
    Reader r(data, length, Message::Definition);
    m.generation = r.Get<uint32_t>();
    m.weapon.id = r.Get<uint16_t>();
    m.weapon.slot = r.Get<uint8_t>();
    m.weapon.order = r.Get<uint8_t>();
    m.weapon.flags = r.Get<uint8_t>();
    r.Text(m.weapon.key);
    r.Text(m.weapon.label);
    r.Text(m.weapon.hud);
    return r.Complete() && m.generation && Valid(m.weapon);
}

bool Decode(const void* data, int length, End& m) {
    Reader r(data, length, Message::End);
    m.generation = r.Get<uint32_t>();
    m.digest = r.Get<uint32_t>();
    return r.Complete() && m.generation;
}

bool Decode(const void* data, int length, State& m) {
    Reader r(data, length, Message::State);
    m.generation = r.Get<uint32_t>();
    m.sequence = r.Get<uint32_t>();
    m.active = r.Get<uint32_t>();
    m.last = r.Get<uint32_t>();
    m.animationSerial = r.Get<uint32_t>();
    m.clip = r.Get<uint16_t>();
    m.reserve = r.Get<uint16_t>();
    m.secondary = r.Get<uint16_t>();
    m.pickup = r.Get<uint16_t>();
    m.subject = r.Get<uint8_t>();
    m.flags = r.Get<uint8_t>();
    m.zoom = r.Get<uint8_t>();
    m.animation = r.Get<uint8_t>();
    m.count = r.Get<uint8_t>();
    if (m.count > MaxOwned) {
        return false;
    }
    for (unsigned i = 0; i < m.count; ++i) {
        m.items[i].instance = r.Get<uint32_t>();
        m.items[i].definition = r.Get<uint16_t>();
    }
    return r.Complete() && Valid(m);
}

bool Decode(const void* data, int length, Kill& m) {
    Reader r(data, length, Message::Kill);
    m.generation = r.Get<uint32_t>();
    m.sequence = r.Get<uint32_t>();
    m.definition = r.Get<uint16_t>();
    m.killer = r.Get<uint8_t>();
    m.victim = r.Get<uint8_t>();
    m.headshot = r.Get<uint8_t>();
    return r.Complete() && Valid(m);
}

uint32_t UpdateDigest(uint32_t digest, const CatalogItem& item) {
    constexpr uint32_t FnvPrime = 16777619u;
    const auto packet = Encode(item);
    for (int i = 0; i < packet.size; ++i) {
        digest = (digest ^ packet.bytes[i]) * FnvPrime;
    }
    return digest;
}

} // namespace cse::weapons
