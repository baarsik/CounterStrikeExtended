#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace cse::weapons {

inline constexpr char UserMessage[] = "CseWeapon";
inline constexpr char CapabilityKey[] = "cseweap";
inline constexpr char CapabilityVersion[] = "1";
inline constexpr char ApiSymbol[] = "CseCustomWeaponsAPI";
inline constexpr uint32_t ApiVersion = 1;
inline constexpr uint8_t ProtocolVersion = 1;
inline constexpr int MaxPacketBytes = 192;
inline constexpr uint16_t MaxDefinitions = 128;
inline constexpr uint16_t MaxAmmoTypes = 128;
inline constexpr uint8_t MaxOwned = 16;
inline constexpr uint8_t MaxHudSlots = 5;
inline constexpr uint8_t MaxPlayers = 32;
inline constexpr uint8_t MaxAnimationSounds = 8;

inline constexpr uint8_t HasSecondary = 1;
inline constexpr uint8_t Scope = 2;
inline constexpr uint8_t SecondaryMode = 1;
inline constexpr uint8_t NoClip = 2;
inline constexpr uint8_t Observing = 4;
inline constexpr uint8_t Reloading = 8;

struct Presentation {
    uint16_t id;
    char key[48];
    char label[48];
    char hud[64];
    uint8_t slot;
    uint8_t order;
    uint8_t flags;
};

struct Ammo {
    char key[32];
    uint16_t capacity;
    uint16_t initialReserve;
    uint8_t stock;
};

struct Definition {
    Presentation presentation;
    Ammo primary;
    Ammo secondary;
    char viewModel[64];
    char playerModel[64];
    char worldModel[64];
    char projectileModel[64];
    char fireSound[64];
    char secondarySound[64];
    char impactSound[64];
    char animationSounds[MaxAnimationSounds][64];
    uint16_t clip;
    uint16_t initialClip;
    uint16_t price;
    uint16_t damage;
    uint16_t secondaryDamage;
    uint16_t secondaryRadius;
    uint16_t projectileSpeed;
    uint16_t fireIntervalMs;
    uint16_t secondaryIntervalMs;
    uint16_t switchIntervalMs;
    uint16_t reloadMs;
    uint16_t deployMs;
    uint16_t range;
    uint16_t speed;
    uint16_t spreadThousandths;
    uint16_t recoilThousandths;
    uint8_t idleAnim;
    uint8_t fireAnim;
    uint8_t reloadAnim;
    uint8_t drawAnim;
    uint8_t secondaryIdleAnim;
    uint8_t secondaryFireAnim;
    uint8_t secondaryEmptyAnim;
    uint8_t secondaryDrawAnim;
    uint8_t switchToSecondaryAnim;
    uint8_t switchToPrimaryAnim;
    uint8_t automatic;
    uint8_t droppable;
    uint8_t zoom;
    uint8_t team;
    uint8_t respawn;
    uint8_t weight;
};

// Server configuration kept outside the versioned backend Definition structure.
struct PrimaryAmmoPurchase {
    uint16_t amount;
    uint16_t price;
};

struct Begin {
    uint32_t generation;
    uint16_t count;
};

struct CatalogItem {
    uint32_t generation;
    Presentation weapon;
};

struct End {
    uint32_t generation;
    uint32_t digest;
};

struct Owned {
    uint32_t instance;
    uint16_t definition;
};

struct State {
    uint32_t generation;
    uint32_t sequence;
    uint32_t active;
    uint32_t last;
    uint32_t animationSerial;
    uint16_t clip;
    uint16_t reserve;
    uint16_t secondary;
    uint16_t pickup;
    uint8_t subject;
    uint8_t flags;
    uint8_t zoom;
    uint8_t animation;
    uint8_t count;
    Owned items[MaxOwned];
};

struct Kill {
    uint32_t generation;
    uint32_t sequence;
    uint16_t definition;
    uint8_t killer;
    uint8_t victim;
    uint8_t headshot;
};

struct Packet {
    unsigned char bytes[MaxPacketBytes];
    int size;
};

enum class Message : uint8_t { Begin = 1, Definition, End, State, Kill };
enum class ParseResult { Ok, InvalidLine, UnknownField, DuplicateField, InvalidValue, MissingField, Incompatible };

bool IsAssetPath(std::string_view path, std::string_view root, std::string_view extension);
std::string HudSpritePath(std::string_view asset);
bool IsWeaponKey(std::string_view key);
ParseResult ParseDefinition(std::string_view text, Definition& definition, unsigned* errorLine = nullptr,
    PrimaryAmmoPurchase* primaryAmmoPurchase = nullptr);

inline constexpr uint32_t InitialDigest = 2166136261u;
uint32_t UpdateDigest(uint32_t digest, const CatalogItem& item);
Message Identify(const void* data, int length);
Packet Encode(const Begin& message);
Packet Encode(const CatalogItem& message);
Packet Encode(const End& message);
Packet Encode(const State& message);
Packet Encode(const Kill& message);
bool Decode(const void* data, int length, Begin& message);
bool Decode(const void* data, int length, CatalogItem& message);
bool Decode(const void* data, int length, End& message);
bool Decode(const void* data, int length, State& message);
bool Decode(const void* data, int length, Kill& message);

enum class Result : int32_t {
    Ok = 1, BackendUnavailable = -1, WrongPhase = -2, InvalidDefinition = -3, Duplicate = -4,
    Capacity = -5, InvalidPlayer = -6, NotReady = -7, NotOwned = -8, Blocked = -9,
    Occupied = -10, InsufficientFunds = -11, MissingAsset = -12, Pending = -13,
};

enum class Operation : uint8_t { Give, Spawn, Equip, Drop, Remove, GrantAmmo, Buy };
enum class SlotPolicy : uint8_t { Reject, Replace, Drop };
enum class EventKind : uint8_t { Spawn, Pickup, Deploy, Holster, ReloadBegin, ReloadComplete, Shot, Drop, Remove, Kill };
enum class Readiness : int32_t { Unsupported, Synchronizing, Ready };

struct Request {
    uint32_t generation;
    uint32_t instance;
    uint32_t transaction;
    int32_t player;
    uint16_t definition;
    uint16_t amount;
    float origin[3];
    SlotPolicy policy;
    uint8_t secondary;
};

struct Event {
    uint32_t generation;
    uint32_t instance;
    uint16_t definition;
    uint16_t clip;
    uint16_t reserve;
    uint16_t secondary;
    uint8_t player;
    uint8_t target;
    EventKind kind;
};

// The provider owns all pointers. Consumers pass caller-owned POD buffers of the negotiated size.
struct Api {
    uint32_t version;
    uint32_t size;
    Result (*registerFile)(const char* path);
    uint16_t (*find)(const char* key);
    Result (*registrationStatus)(const char* key);
    int32_t (*readiness)(int32_t player);
    Result (*operate)(Operation operation, const Request* request, uint32_t* instance);
    Result (*query)(int32_t player, State* state);
    bool (*pollEvent)(Event* event);
    uint32_t (*generation)();
    Result (*definition)(uint16_t id, Definition* out);
};

using GetApi = const Api* (*)(uint32_t version, uint32_t size);

} // namespace cse::weapons
