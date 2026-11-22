#ifndef CSE_CUSTOM_WEAPON_CLIENT_H
#define CSE_CUSTOM_WEAPON_CLIENT_H

#include "../../public/cse_custom_weapons.h"
#include "wrect.h"
#include "cl_dll.h"

namespace cse::weapons {

enum class SpriteRole { Weapon, Selected, Ammo, SecondaryAmmo, Crosshair, Zoom, Death, SecondaryCrosshair, Count };

struct HudSprite {
    HSPRITE handle{};
    wrect_t rect{};

    int Width() const { return rect.right - rect.left; }
    int Height() const { return rect.bottom - rect.top; }
};

HSPRITE LoadHudDescriptorSprite(std::string_view asset);

void RegisterWeaponClient();
void ResetWeaponSession();
void RefreshWeaponArt();
void RunWeaponClientFrame();
void ReadWeaponDemoPacket(const void* data, int size);
const State* LocalWeapons();
const State* ActiveWeaponView();
const Presentation* WeaponPresentation(uint16_t definition);
const Presentation* ActiveWeaponPresentation(const State& state);
HudSprite WeaponSprite(uint16_t definition, SpriteRole role);
void SelectCustomWeapon(uint32_t instance);
void DrawPrimaryAmmo(int y, int digitWidth, int clip, int reserve, const HudSprite& icon, int alpha);
void DrawCustomAmmo(const State& state, const Presentation& weapon, int alpha);
int MoneyYAboveSecondaryAmmo(int stockY);

} // namespace cse::weapons

#endif
