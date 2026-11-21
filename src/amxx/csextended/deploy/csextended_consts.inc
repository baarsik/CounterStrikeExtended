#if defined _csextended_consts_included
    #endinput
#endif
#define _csextended_consts_included

/**
 * Stock menus supported by cse_forms_override_menu().
 */
enum CseFormMenu {
    CseFormMenu_Team = 1,
    CseFormMenu_TerroristClass,
    CseFormMenu_CounterTerroristClass,
    CseFormMenu_Buy,
    CseFormMenu_BuyEquipment
};

/**
 * @section Custom weapon constants
 */

/** API version reported by a compatible GameDLL. */
#define CSE_WEAPON_API_VERSION 1

/** Number of cells required by each cse_weapons_state() inventory array. */
#define CSE_WEAPON_MAX_OWNED 16

/**
 * Result codes returned by the custom weapon natives.
 */
enum CseWeaponResult {
    CseWeapon_Ok = 1,              // Operation completed or definition submitted.
    CseWeapon_NoBackend = -1,      // Active GameDLL has no compatible provider.
    CseWeapon_WrongPhase = -2,     // Registration closed, map inactive, or stale generation.
    CseWeapon_BadDefinition = -3,  // Unknown or malformed definition.
    CseWeapon_Duplicate = -4,      // Another definition already uses this key.
    CseWeapon_Capacity = -5,       // A registry, resource, or queue limit was reached.
    CseWeapon_BadPlayer = -6,      // Invalid player for this operation.
    CseWeapon_NotReady = -7,       // Player cannot currently use custom weapons.
    CseWeapon_NotOwned = -8,       // Instance is gone or is held by another player.
    CseWeapon_Blocked = -9,        // Argument or operation rejected by gameplay rules.
    CseWeapon_Occupied = -10,      // Equipment category already occupied.
    CseWeapon_NoFunds = -11,       // Player cannot afford the purchase.
    CseWeapon_MissingAsset = -12,  // A required server asset is missing or invalid.
    CseWeapon_Pending = -13        // Pending catalog commit or deferred gameplay request.
};

/**
 * Policies for an occupied equipment slot when giving or buying a weapon.
 */
enum CseWeaponSlotPolicy {
    CseWeaponSlot_Reject = 0,   // Refuse a grant or purchase that would displace a weapon.
    CseWeaponSlot_Replace,      // Remove the displaced weapon.
    CseWeaponSlot_Drop          // Drop the displaced weapon as a world pickup.
};

/**
 * Client states reported by cse_weapons_readiness().
 */
enum CseWeaponReadiness {
    CseWeaponClient_Unsupported = 0,  // No custom weapon support has been established.
    CseWeaponClient_Synchronizing,     // The client is receiving the map catalog.
    CseWeaponClient_Ready              // The client acknowledged the map catalog.
};

/**
 * Array indices for the metadata returned by cse_weapons_definition().
 */
enum CseWeaponDefinition {
    CseWeaponDef_Slot,               // HUD slot, 0 for primary or 1 for secondary.
    CseWeaponDef_Order,              // Sort order within the HUD slot.
    CseWeaponDef_Flags,              // Bitmask of CseWeaponDefinitionFlags.
    CseWeaponDef_ClipCapacity,       // Magazine capacity, or 0 for a clipless weapon.
    CseWeaponDef_ReserveCapacity,
    CseWeaponDef_SecondaryCapacity,  // Secondary reserve capacity, or 0 if absent.
    CseWeaponDef_Price               // Purchase cost, or 0 if unavailable for purchase.
};

/**
 * Flags stored in CseWeaponDef_Flags.
 */
enum CseWeaponDefinitionFlags {
    CseWeaponDefFlag_Secondary = 1,
    CseWeaponDefFlag_Scope = 2
};

/**
 * Array indices for the snapshot returned by cse_weapons_state().
 */
enum CseWeaponState {
    CseWeaponState_Generation,
    CseWeaponState_Active,      // Active custom instance identifier, or 0.
    CseWeaponState_Last,        // Last custom instance identifier, or 0.
    CseWeaponState_Clip,        // Active weapon's magazine contents.
    CseWeaponState_Reserve,     // Active weapon's primary reserve.
    CseWeaponState_Secondary,   // Active weapon's secondary reserve.
    CseWeaponState_Pickup,      // Reserved for pickup notifications. Always 0 in a Pawn state query.
    CseWeaponState_Subject,     // Client index described by the snapshot.
    CseWeaponState_Flags,       // Bitmask of CseWeaponStateFlags.
    CseWeaponState_Zoom,        // Active weapon's field of view in degrees, or 0 if inactive.
    CseWeaponState_Count        // Number of populated entries in each inventory array.
};

/**
 * Flags stored in CseWeaponState_Flags.
 */
enum CseWeaponStateFlags {
    CseWeaponFlag_SecondaryMode = 1,
    CseWeaponFlag_NoClip = 2,
    CseWeaponFlag_Observing = 4,
    CseWeaponFlag_Reloading = 8
};

/**
 * Event kinds passed to cse_weapons_event().
 */
enum CseWeaponEvent {
    CseWeaponEvent_Spawn = 0,
    CseWeaponEvent_Pickup,
    CseWeaponEvent_Deploy,
    CseWeaponEvent_Holster,
    CseWeaponEvent_ReloadBegin,
    CseWeaponEvent_ReloadComplete,
    CseWeaponEvent_Shot,
    CseWeaponEvent_Drop,
    CseWeaponEvent_Remove,
    CseWeaponEvent_Kill
};

/**
 * @endsection
 */
