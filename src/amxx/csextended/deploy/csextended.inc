#if defined _csextended_included
    #endinput
#endif
#define _csextended_included
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
 * Registers a form from <amxx_datadir>/csextended/forms/<name>.form for this map.
 * Call from plugin_precache. Choose a lowercase asset name with letters,
 * digits, hyphens, or underscores.
 *
 * @param name       Bare form name, without the .form extension.
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
 * @param control    One-based control index in the form's declaration order.
 * @param value      Control value as a string. Buttons use an empty string,
 *                   checks use 0 or 1, radios use 1, sliders use decimal integers,
 *                   dropdowns use zero-based indices, and text fields use UTF-8.
 */
forward cse_forms_action(player, const form[], control, const value[]);
