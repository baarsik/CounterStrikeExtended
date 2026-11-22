#include <amxmodx>
#include <amxmisc>
#include <csextended>

new const WeaponKey[] = "weapon_svdex";
new g_purchaseToken;

public plugin_precache() {
    // The SVD definition supplies the HUD descriptor and its optional WebP death sprite.
    new CseWeaponResult:result = cse_weapons_register(WeaponKey);
    if (result != CseWeapon_Ok) {
        log_amx("Could not submit %s (%d)", WeaponKey, result);
    }
}

public plugin_init() {
    register_plugin("CSE Custom Weapon Example", "1.0.0", "CounterStrikeExtended");
    register_clcmd("cse_give_svd", "GiveWeapon", ADMIN_CFG);
    register_clcmd("cse_spawn_svd", "SpawnWeapon", ADMIN_CFG);
    register_clcmd("say /svd", "EquipWeapon");
    register_clcmd("say /buy_svd", "BuyWeapon");
}

public cse_weapons_ready(player, generation) {
    if (generation == cse_weapons_generation() && cse_weapons_find(WeaponKey)) {
        client_print(player, print_chat, "SVD available. Say /buy_svd to buy or /svd to equip.");
    }
}

public GiveWeapon(player, level, cid) {
    if (!cmd_access(player, level, cid, 1)) {
        return PLUGIN_HANDLED;
    }
    new instance;
    new CseWeaponResult:result = cse_weapons_give(player, cse_weapons_generation(), cse_weapons_find(WeaponKey),
        instance);
    console_print(player, "SVD grant result %d, instance %d", result, instance);
    return PLUGIN_HANDLED;
}

public SpawnWeapon(player, level, cid) {
    if (!cmd_access(player, level, cid, 1) || !is_user_alive(player)) {
        return PLUGIN_HANDLED;
    }
    new aim[3], Float:position[3], instance;
    get_user_origin(player, aim, 3);
    IVecFVec(aim, position);
    position[2] += 16.0;
    new CseWeaponResult:result = cse_weapons_spawn(cse_weapons_generation(), cse_weapons_find(WeaponKey),
        position, instance);
    console_print(player, "SVD spawn result %d, instance %d", result, instance);
    return PLUGIN_HANDLED;
}

public EquipWeapon(player) {
    new snapshot[CseWeaponState], instances[CSE_WEAPON_MAX_OWNED], definitions[CSE_WEAPON_MAX_OWNED];
    if (cse_weapons_state(player, snapshot, instances, definitions) == CseWeapon_Ok) {
        new definition = cse_weapons_find(WeaponKey);
        for (new i = 0; i < snapshot[CseWeaponState_Count]; ++i) {
            if (definitions[i] == definition) {
                cse_weapons_equip(player, snapshot[CseWeaponState_Generation], instances[i]);
                return PLUGIN_HANDLED;
            }
        }
    }
    client_print(player, print_chat, "You do not have the SVD ready to equip.");
    return PLUGIN_HANDLED;
}

public BuyWeapon(player) {
    new instance;
    // In a shop, retain the token when retrying the same purchase request.
    new CseWeaponResult:result = cse_weapons_buy(player, cse_weapons_generation(), cse_weapons_find(WeaponKey),
        ++g_purchaseToken, instance);
    client_print(player, print_chat, "SVD purchase result %d", result);
    return PLUGIN_HANDLED;
}
