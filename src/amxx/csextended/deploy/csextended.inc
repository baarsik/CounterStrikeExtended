#if defined _csextended_included
    #endinput
#endif
#define _csextended_included

#include <csextended_consts>

#pragma reqlib csextended
#if !defined AMXMODX_NOAUTOLOAD
    #pragma loadlib csextended
#endif

// Extended health and armor.

/**
 * Sets a player's health and queues a HUD update from the actual entity value.
 *
 * @note Nonpositive health invokes the game DLL's normal player death path.
 *       Positive health does not respawn a dead player.
 * @note The game stores health as a float. Integers above 16777216 may lose
 *       precision. The client retains the transmitted 32-bit amount, while
 *       the HUD displays at most 99999.
 * @note Calls from message hooks or publication callbacks can be deferred
 *       until the next safe server frame, including player death.
 *
 * @param index     Client index as an integer in the range 1 to MaxClients.
 *                  The player must be connected; bots are accepted.
 * @param health    New health as a signed 32-bit integer. Zero or negative
 *                  values kill the player; positive values set health.
 *
 * @return          1 if the request was accepted.
 * @error           If index is out of range, disconnected, or an HLTV proxy.
 */
native cse_set_user_health(index, health);

/**
 * Sets a player's armor amount and queues a HUD update from the entity value.
 *
 * @note This changes the amount only. Use the CStrike equipment API to grant
 *       or change kevlar and helmets.
 * @note The game stores armor as a float. Integers above 16777216 may lose
 *       precision. The client retains the transmitted 32-bit amount, while
 *       the HUD displays at most 99999.
 * @note Calls from message hooks or publication callbacks can be deferred
 *       until the next safe server frame.
 *
 * @param index     Client index as an integer in the range 1 to MaxClients.
 *                  The player must be connected; bots are accepted.
 * @param armor     New armor as a signed 32-bit integer in the range
 *                  0 to 2147483647. Zero removes the remaining armor points.
 *
 * @return          1 if the request was accepted.
 * @error           If index is out of range, disconnected, or an HLTV proxy,
 *                  or if armor is negative.
 */
native cse_set_user_armor(index, armor);

/**
 * Queues a health and armor update using the player's current entity values.
 *
 * @note Use this after another API changes health or armor, including when
 *       automatic observation is disabled with cse_extstats_auto 0.
 *       Transmission waits until a safe server frame.
 *
 * @param index     Client index as an integer in the range 1 to MaxClients.
 *                  The player must be connected; bots are accepted.
 *
 * @return          1 if the synchronization request was accepted.
 * @error           If index is out of range, disconnected, or an HLTV proxy.
 */
native cse_sync_user_stats(index);

/**
 * Returns whether the client has been identified as CounterStrikeExtended.
 *
 * @note Detection is asynchronous. This returns false until the client has
 *       answered the CSE identity query successfully.
 * @note CSE identity and extended-stats protocol support are detected
 *       separately. A true result does not guarantee a protocol version.
 *
 * @param index     Client index as an integer in the range 1 to MaxClients.
 *
 * @return          Boolean true for an identified CSE client; false for
 *                  unknown or unsupported clients, invalid or disconnected
 *                  indices, bots, and HLTV proxies.
 */
native bool:cse_is_supported(index);

// Server forms and images.

/**
 * Registers a form from <amxx_datadir>/csextended/forms/<name>.xaml for this map.
 * Call from plugin_precache. Choose a lowercase asset name with letters,
 * digits, hyphens, or underscores.
 *
 * @param name       Bare form name, without the .xaml extension.
 * @return           True when the form is registered and valid.
 */
native bool:cse_forms_precache_form(const name[]);

/**
 * Registers an image from <amxx_datadir>/csextended/forms/ for this map.
 * Supply the bare asset name. The module tries .rgba, then .webp if .rgba is absent.
 * Call from plugin_precache. Persistent images can be reused from this
 * server's client cache on a later connection.
 *
 * @param name       Bare image name, without the .rgba or .webp extension.
 * @param persistent True allows reuse from this server's client cache across
 *                   connections. False keeps the image in the current session.
 * @return           True when the image is registered and valid.
 */
native bool:cse_forms_precache_image(const name[], bool:persistent = true);

/**
 * Registers a map-wide replacement for a Counter-Strike stock menu in both
 * VGUI and old-style menu modes. Call from plugin_precache after registering
 * the form. The last successful registration for a menu takes precedence.
 * Clients use the stock menu until they can display the replacement.
 *
 * @param menu       Stock team, class, buy, or equipment menu to replace.
 * @param name       Bare name of an already precached form.
 * @return           True when registered for the current map.
 */
native bool:cse_forms_override_menu(CseFormMenu:menu, const name[]);

/**
 * Queues a precached form for a connected client once the layout and every
 * referenced image are ready to display. Replaces any existing form.
 *
 * @param player     Client index of the connected player receiving the form.
 * @param name       Bare name of a precached form.
 * @return           True when queued. Use an ordinary menu or retry when false.
 */
native bool:cse_forms_show(player, const name[]);

/**
 * Cancels a queued show or closes the player's current server form.
 *
 * @param player     Client index of the connected player whose form is dismissed.
 * @return           True when a form was queued or open.
 */
native bool:cse_forms_hide(player);

/**
 * Reports whether a client negotiated forms and finished synchronizing the
 * map's assets. Check cse_forms_show to verify availability of a specific form.
 *
 * @param player     Client index of the connected player to check.
 * @return           True when synchronization has finished for that player.
 */
native bool:cse_forms_ready(player);

/**
 * Receives an action from an interactive control on the player's current form.
 * Apply normal server-side authorization before changing game state.
 *
 * @param player     Client index of the player who submitted the action.
 * @param form       Bare name of the open form.
 * @param control    One-based index of a rendered control in declaration order.
 * @param value      Control value as a string. Buttons use an empty string,
 *                   checks and toggles use 0 or 1, radios use 1, sliders use decimal
 *                   integers, lists and dropdowns use zero-based indices, and
 *                   text fields use UTF-8.
 *
 * @noreturn
 */
forward cse_forms_action(player, const form[], control, const value[]);

/**
 * @section Custom weapons
 *
 * A compatible Counter-Strike GameDLL is required. Weapon results are returned
 * as CseWeaponResult values. Generation and instance identifiers are opaque
 * 32-bit cells. Preserve every bit and compare them for equality.
 *
 * Gameplay requests from cse_weapons_ready() or cse_weapons_event() may be
 * queued for a later frame and return CseWeapon_Pending. Use a later event
 * or cse_weapons_state() to confirm completion.
 */

/**
 * Returns the active custom weapon API version.
 *
 * @return          CSE_WEAPON_API_VERSION if a compatible GameDLL is available,
 *                  0 otherwise
 */
native cse_weapons_api_version();

/**
 * Returns the custom weapon catalog generation.
 *
 * @note Store this value with definition and instance identifiers and look them
 *       up again when the generation changes.
 *
 * @return          Catalog generation, or 0 if no generation is available
 */
native cse_weapons_generation();

/**
 * Submits a custom weapon definition from the AMXX data directory.
 *
 * @note Call from plugin_precache(). The file is read from
 *       <amxx_datadir>/csextended/weapons/<basename>.cfg.
 * @note A successful submission is staged. Call
 *       cse_weapons_registration_status() to check whether it was committed.
 *
 * @param basename  Bare lowercase weapon key such as "weapon_svdex"
 *
 * @return          CseWeapon_Ok if staged, or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_register(const basename[]);

/**
 * Looks up a committed weapon definition by its key.
 *
 * @param key  Canonical weapon key such as "weapon_svdex"
 *
 * @return     Definition identifier on success, 0 if unavailable
 */
native cse_weapons_find(const key[]);

/**
 * Returns the registration status of a custom weapon definition.
 *
 * @param key  Canonical weapon key
 *
 * @return     CseWeapon_Ok if committed, CseWeapon_Pending if staged,
 *             or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_registration_status(const key[]);

/**
 * Returns a client's custom weapon readiness.
 *
 * @note Wait for CseWeaponClient_Ready before granting a custom loadout.
 *
 * @param player  Client index
 *
 * @return        A CseWeaponReadiness value, CseWeapon_NoBackend if the
 *                GameDLL is unavailable, or CseWeapon_BadPlayer if the
 *                index is outside the client range
 */
native cse_weapons_readiness(player);

/**
 * Retrieves the key, display label, and metadata of a committed definition.
 *
 * @param definition    Definition identifier from cse_weapons_find()
 * @param key           Buffer to store the canonical key
 * @param key_maxlen    Maximum key length, excluding the null terminator
 * @param label         Buffer to store the UTF-8 display label
 * @param label_maxlen  Maximum label length, excluding the null terminator
 * @param metadata      Array indexed by CseWeaponDefinition
 * @param generation    Catalog generation associated with the definition
 *
 * @return              CseWeapon_Ok on success, or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_definition(definition, key[], key_maxlen, label[], label_maxlen,
    metadata[CseWeaponDefinition], generation);

/**
 * Retrieves a player's custom weapon state and inventory.
 *
 * @note Read the first snapshot[CseWeaponState_Count] entries of both inventory
 *       arrays. The remaining entries are cleared on success.
 * @note Ammo counts describe the active custom weapon. The active and last
 *       identifiers are 0 when those roles have no custom instance.
 *
 * @param player       Client index
 * @param snapshot     Array indexed by CseWeaponState
 * @param instances    Buffer to store owned instance identifiers
 * @param definitions  Buffer to store the corresponding definition identifiers
 *
 * @return             CseWeapon_Ok on success, or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_state(player, snapshot[CseWeaponState], instances[CSE_WEAPON_MAX_OWNED],
    definitions[CSE_WEAPON_MAX_OWNED]);

/**
 * Gives a custom weapon to a ready player.
 *
 * @param player      Client index
 * @param generation  Catalog generation associated with the definition
 * @param definition  Definition identifier
 * @param instance    Variable to store the instance identifier on success
 * @param policy      Policy for an occupied equipment slot
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_give(player, generation, definition, &instance,
    CseWeaponSlotPolicy:policy = CseWeaponSlot_Reject);

/**
 * Spawns a custom weapon as a world pickup.
 *
 * @param generation  Catalog generation associated with the definition
 * @param definition  Definition identifier
 * @param origin      Pickup position as an X, Y, Z vector
 * @param instance    Variable to store the instance identifier on success
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_spawn(generation, definition, const Float:origin[3], &instance);

/**
 * Selects a custom weapon through the GameDLL's weapon switching rules.
 *
 * @param player      Client index of the weapon's owner
 * @param generation  Catalog generation associated with the instance
 * @param instance    Owned instance identifier
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_equip(player, generation, instance);

/**
 * Drops a custom weapon from its owner's inventory.
 *
 * @param player      Client index of the weapon's owner
 * @param generation  Catalog generation associated with the instance
 * @param instance    Owned instance identifier
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_drop(player, generation, instance);

/**
 * Removes a custom weapon from its owner's inventory and the world.
 *
 * @param player      Client index of the weapon's owner
 * @param generation  Catalog generation associated with the instance
 * @param instance    Owned instance identifier
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_remove(player, generation, instance);

/**
 * Adds rounds to a player's reserve for a custom weapon.
 *
 * @note The resulting reserve is capped at the selected ammo pool's capacity.
 *       Weapons sharing that pool see the same reserve.
 *
 * @param player      Client index of the weapon's owner
 * @param generation  Catalog generation associated with the instance
 * @param instance    Owned instance identifier
 * @param amount      Number of rounds to add, from 0 to 65535
 * @param secondary   True to add to secondary ammo, false for primary ammo
 *
 * @return            CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                    or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_grant_ammo(player, generation, instance, amount, bool:secondary = false);

/**
 * Purchases a custom weapon through the GameDLL's buy rules.
 *
 * @note Use a unique nonzero transaction value for each independent purchase.
 *       When retrying a purchase, reuse the same value, definition, and policy.
 *       Transaction values are shared by all callers for a player on the
 *       current connection and map.
 *
 * @param player       Client index
 * @param generation   Catalog generation associated with the definition
 * @param definition   Definition identifier
 * @param transaction  Transaction identifier, or 0 to skip duplicate detection
 * @param instance     Variable to store the instance identifier on success
 * @param policy       Policy for an occupied equipment slot
 *
 * @return             CseWeapon_Ok on success, CseWeapon_Pending if queued,
 *                     or a CseWeaponResult failure code
 */
native CseWeaponResult:cse_weapons_buy(player, generation, definition, transaction, &instance,
    CseWeaponSlotPolicy:policy = CseWeaponSlot_Reject);

/**
 * Called when a client becomes ready for the current weapon catalog.
 *
 * @param player      Client index
 * @param generation  Generation of the catalog synchronized by the client
 *
 * @noreturn
 */
forward cse_weapons_ready(player, generation);

/**
 * Called when the module delivers a committed custom weapon event.
 *
 * @param kind        Event type from CseWeaponEvent
 * @param generation  Catalog generation
 * @param instance    Weapon instance identifier
 * @param definition  Weapon definition identifier
 * @param player      Client index of the actor or owner, or 0
 * @param target      Client index of the affected player, or 0
 * @param clip        Magazine contents at the time of the event
 * @param reserve     Primary reserve at the time of the event
 * @param secondary   Secondary reserve at the time of the event
 *
 * @noreturn
 */
forward cse_weapons_event(CseWeaponEvent:kind, generation, instance, definition, player, target, clip, reserve,
    secondary);

/**
 * @endsection
 */
