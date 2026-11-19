# Server forms

CounterStrikeExtended clients can display native VGUI2 forms provided by a Counter-Strike or Condition Zero server running the `csextended` AMXX module. A Pawn plugin registers a form and its images at map precache, then opens it by name. The server receives typed control actions and decides what they do. Stock clients can use the ordinary AMXX menu provided by the plugin as a fallback.

## Try the sample

1. Build with the `amxx-win32-release` or `amxx-linux32-release` configure and build presets. Install Metamod and AMX Mod X on the server, then copy `build/<preset>/package/addons/amxmodx/` into the server's game directory.
2. Add `csextended` to `addons/amxmodx/configs/modules.ini`. Follow the [upgrade instructions](extended-stats.md#build-and-install) if you have the two previous modules. Compile `addons/amxmodx/scripting/cse_forms_example.sma` with the server's AMXX compiler, put the resulting `.amxx` under `addons/amxmodx/plugins/`, and list it in `plugins.ini`.
3. Restart the server. Connect with CSE and type `/cse_form` in chat. Try the main keyboard or keypad digits, letter shortcuts, and mouse. Focus the preview title, enter text, then press Enter to submit it. The plugin reports selections in the console. The same chat command opens a numbered menu on a stock client.

`addons/amxmodx/configs/csextended.cfg` sets `cse_forms_require_client 0` by default. Set it to `1` to reject connecting network players without the CSE server-forms capability marker. This is an admission compatibility check. A modified client can implement or impersonate the protocol, so continue using normal server authentication and access controls. CSE players can disable server forms with `cl_cse_server_forms 0` or `cl_allowdownload 0` and reconnect to use plugin fallbacks.

## Register and open forms

Place assets in `csextended/forms/` under the server's AMXX data directory (`addons/amxmodx/data/` by default). Move existing assets from the previous `data/cse_forms/` directory when upgrading. Names are unique across assets on a map, use lowercase ASCII letters, digits, hyphens or underscores, and are at most 24 characters. Use `<name>.form` for a layout and `<name>.rgba` or `<name>.webp` for an image. Register and reference images using their bare names. When both extensions exist for a name, `.rgba` takes precedence.

```pawn
#include <csextended>

public plugin_precache() {
    cse_forms_precache_form("shop");
    cse_forms_precache_image("shop-icon", true);
    cse_forms_precache_image("map-preview", false);
}

public open_shop(player) {
    if (!cse_forms_show(player, "shop")) {
        // Display the plugin's ordinary AMXX menu here.
    }
}

public cse_forms_action(player, const form[], control, const value[]) {
    // Validate the player's access and state before applying an action.
}
```

`cse_forms_show` returns true when a form and all its images are ready and the show was queued. `cse_forms_ready` reports when synchronization has finished. Use `cse_forms_hide(player)` to dismiss a queued or displayed form. A client dismissal invalidates the open instance.

## Replace stock menus

Register each replacement in `plugin_precache` after precaching its layout. The same registration applies in VGUI and old-style menu modes.

```pawn
public plugin_precache() {
    if (cse_forms_precache_form("team-picker")) {
        cse_forms_override_menu(CseFormMenu_Team, "team-picker");
    }
    if (cse_forms_precache_form("shop")) {
        cse_forms_override_menu(CseFormMenu_Buy, "shop");
    }
}
```

Supported menus are team selection, Terrorist and Counter-Terrorist class selection, the main buy menu, and the equipment menu. `CseFormMenu_Buy` also covers pressing the buy key, which opens the VGUI menu from the client. The equipment menu is reached through `buyequip` or the stock old-style buy menu. VGUI weapon categories inside the stock buy panel are part of that panel.

The stock menu opens while the client requests its replacement and remains usable if the request expires or the form is unavailable. Clients without override support, clients still synchronizing assets, and clients with downloads or forms disabled use the stock menus.

Handle selections through `cse_forms_action` as usual. Map each control to a specific choice and apply the game's rules for joining a team, selecting a class, or purchasing an item. A replacement form action alone grants no gameplay permissions.

## Layout and image formats

A form is UTF-8 text. Blank lines and lines beginning with `#` after the header are ignored. Use single spaces between the fixed fields. The remaining text is taken literally.

```text
form 480 260 Server shop
group 12 12 456 226 - 0 0 Equipment
image 30 55 48 48 - 0 0 shop-icon
label 90 55 350 24 - 0 0 Equipment preview
button 90 90 172 28 1 0 0 Buy armor
button 272 90 160 28 Q 0 0 Close
```

There are eight fixed fields on each control line before its text. They are the type, x, y, width, height, shortcut, first parameter, and second parameter. Geometry uses the form's content area. Width can be 240–800 and height 180–600. Use `-` for an unbound shortcut. ASCII letters match either case. A digit also works on the numeric keypad. Each shortcut may be bound once.

| Type | First and second parameters | Text and action |
| --- | --- | --- |
| `group`, `label`, `divider` | `0 0` | Display text. A divider may omit its text. |
| `image` | `0 0` | Precached image name. |
| `button` | `0 0` | Caption. The action value is empty. |
| `check` | Initial `0` or `1`, then `0` | Caption. Actions return `0` or `1`. |
| `radio` | Group 1–9, then initial `0` or `1` | Caption. Selecting it returns `1`. |
| `slider` | Minimum and maximum in 0–10000 | Begins at the minimum. Actions return a decimal integer. |
| `select` | Initial zero-based choice, then `0` | Up to eight labels separated by `\|`. Actions return a zero-based index. |
| `text` | `0 0` | Initial text. Enter submits up to 48 UTF-8 bytes. |

The callback receives a one-based control index in declaration order. `.rgba` images use two little-endian 16-bit dimensions followed by row-major RGBA bytes. Static `.webp` images may be lossy or lossless and can include transparency. Both formats are limited to 1–64 pixels per dimension and 16,388 bytes per image file. A map can register up to 32 assets and 128 KiB total, with a maximum of 48 controls and 8 KiB per form. Older CSE clients can display RGBA-backed forms and use the plugin's fallback for forms that reference WebP.

## Asset isolation

Forms and session images live in memory and are discarded on map reset, disconnect, or server switch. Demo recording omits server-form messages. Persistent images use a private application cache outside GoldSrc's game search paths. CSE derives each cache namespace from the connected network endpoint, derives filenames from SHA-256 digests, and requires proof over the entire image using a fresh challenge before reusing a cached file. The server protocol has no cache listing or file upload operation. A successful proof requires the same bytes even if another operator later uses the same endpoint.

Only validated images enter the persistent cache. WebP images are transferred and cached as compressed files and decoded to RGBA for display. The cache holds up to 32 images per endpoint and 128 endpoint namespaces. When the cache is full or unavailable, downloaded images remain usable in memory. The form renderer accepts the controls listed above and treats captions and image references as data. Each submission uses the current form instance, a declared control index and a validated value. An AMXX plugin remains responsible for gameplay permissions.

## Developer checks

```sh
cmake -S tests/native/server_forms -B build/server-forms-tests -DCSE_SERVER_FORMS_SANITIZERS=ON
cmake --build build/server-forms-tests
ctest --test-dir build/server-forms-tests --output-on-failure
```
