#if defined _cse_extstats_included
    #endinput
#endif
#define _cse_extstats_included
#pragma reqlib cse_extstats
#if !defined AMXMODX_NOAUTOLOAD
    #pragma loadlib cse_extstats
#endif

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
