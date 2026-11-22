# Custom weapons

A custom weapon has its own classname, inventory identity, and ammo definition. The Counter-Strike GameDLL is authoritative. CounterStrikeExtended clients receive the catalog and display custom inventory, ammo, models and kill notices while stock weapons continue to use the legacy protocol.

## SVD Custom test fixture

Use a separate test server and an updated CounterStrikeExtended client. From the repository root, run the installer against the `cstrike` directory on both the test server and the test client. It reads the supplied archive and installs its model, sound and sprite files, plus the CSE definition. A repeated import is safe when the files match. The archive itself stays outside the repository.

```sh
python3 tools/qualification/fixtures/custom-weapons/install_fixture.py /path/to/weapon_svdex.zip /path/to/cstrike \
  --killfeed-webp /path/to/svdex-killfeed.webp
```

The WebP argument installs the optional killfeed icon used by the Pawn example and the GameDLL catalog. The example uses the 48 × 16 `d_svdex` image from the CSMoE HUD atlas at `(172, 0)`. [CSMoE HUD mapping](https://github.com/MoeMod/CSMoE-GameDir/blob/e8352a9f0dec86c12960b3f7730455eac9cd1221/csmoe/sprites/hud.txt#L167)

For a cold start, add `-cseweaponcatalog addons/amxmodx/data/csextended/weapons/weapon_svdex.cfg +map de_dust2` to the server arguments. On an already running server, set the catalog and change the map to load it. Use the server console to give the weapon to a player. `status` lists connected players and `cse_weapon_list` lists registered definitions.

```text
cse_weapon_catalog addons/amxmodx/data/csextended/weapons/weapon_svdex.cfg
changelevel de_dust2
cse_weapon_list
cse_weapon_give 1 weapon_svdex replace
```

Select the SVD through the normal weapon controls or with the console command `weapon_svdex`. It starts with 20 rounds in the rifle magazine, 180 in the rifle reserve, and 10 launcher rounds in a separate pool. The launcher count is the blue row above the normal ammo display. Secondary attack changes mode, and primary attack fires the selected mode. Reload replenishes the rifle magazine from the rifle reserve. The standard primary ammo buy command replenishes up to 30 rifle rounds for $80 when buying is allowed. These values and the combat balance are fixture settings for CSE multiplayer.

The world-spawn command is `cse_weapon_spawn <weapon-key> <x> <y> <z>`. The `cse_weapon_give` policy argument is `reject`, `replace`, or `drop`. In standard rules a player has one primary and one secondary weapon. Set `cse_weapon_shared_slots 1` before testing two independent custom guns in one slot. Set `cse_weapon_require_client 1` for games that require clients with support for the custom weapon protocol.

## Definitions

A definition is a UTF-8 `.cfg` file with one `key=value` field per line. Whitespace around field names and values is ignored. `#` at the start of a line marks a comment. Unknown or duplicate fields are rejected. A `.lst` catalog lists game-relative definition files, one per line, and permits the same comments. Registration takes place during map loading so all assets can be precached. Use the shipped configuration as a worked example. [SVD definition](../tools/qualification/fixtures/custom-weapons/weapon_svdex.cfg)

| Group | Fields |
| --- | --- |
| Identity | `version=1`, `key`, `label`, `slot` (0 primary or 1 secondary), `order`, `weight` |
| Ammo | Required `primary` (`custom:<name>` or `stock:<name>`) and `clip`. Private pools also require `primary_capacity`. Optional fields are `initial_clip` and `primary_reserve`. Set `clip=0` to spend the reserve directly. Set both `primary_buy_amount` and `primary_buy_price` to enable standard purchases for the primary reserve. A launcher also uses `secondary=custom:<name>`, `secondary_capacity` and `secondary_reserve`. Reuse a pool name with the same capacity to share reserve between definitions. |
| Models and sound | Required `view`, `player`, and `world` model paths. A launcher requires `projectile`. Optional `fire_sound`, `secondary_sound`, `impact_sound`, and `animation_sound_1` through `animation_sound_8` refer to WAV files under `sound/weapons/`, using `weapons/` in the definition. |
| Firing and movement | `automatic`, `damage`, `range`, `speed`, `spread` and `recoil` (thousandths), `fire_ms`, `reload_ms`, `deploy_ms`, `zoom`. Launcher options are `secondary_damage`, `secondary_radius`, `projectile_speed`, `secondary_ms` and `switch_ms`. |
| Animations and HUD | `idle_anim`, `fire_anim`, `reload_anim`, `draw_anim`, optional launcher `secondary_idle_anim`, `secondary_fire_anim`, `secondary_empty_anim`, `secondary_draw_anim`, `switch_to_secondary_anim`, `switch_to_primary_anim`, and `hud` for a sprite descriptor. |
| Rules | `droppable`, `respawn`, `price`, and `team` (0 any, 1 Terrorist, 2 Counter-Terrorist). |

In the HUD descriptor, `crosshair`, `crosshair2`, and `zoom` provide optional primary, launcher, and zoomed reticles. Without primary reticle art, the client draws the configured Counter-Strike crosshair.

Sprite rows accept filenames relative to `sprites/`, using either `.webp` or `.spr`. Extensionless names resolve to `.spr` as before. WebP images use the same pixel rectangle as sprite atlases and can be mixed with legacy sprites in one descriptor. Use static WebP images up to 1024 × 1024 with transparency for the cutout. The `death` row supplies the killfeed icon.

Install the reference GameDLL with the CSE client. The GameDLL can load its catalog directly. On a server stack with a compatible AMX Mod X backend, include `csextended` and register `weapon_svdex` during `plugin_precache` to read the same definition from `csextended/weapons/` under the AMXX data directory. Choose either the GameDLL catalog or the Pawn registration for a definition. The Pawn API reports backend availability and readiness separately. The in-tree GameDLL's initialization requires CSE's checked module ABI, so positive AMXX gameplay qualification requires a CSE-aware Metamod adapter or a separate backend port to an AMXX-supported GameDLL. [Pawn include](../src/amxx/csextended/deploy/csextended.inc) · [Example plugin](../src/amxx/csextended/deploy/examples/weapons.sma)

Weapon readiness currently confirms protocol and catalog synchronization. Install fixture assets on both sides for testing. Automatic delivery and per-client verification of custom assets, exact per-map precache headroom, and live qualification of in-eye spectators remain open.
