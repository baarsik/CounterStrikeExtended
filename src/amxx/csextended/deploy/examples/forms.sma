#include <amxmodx>
#include <csextended>

new const ShowcaseForm[] = "showcase";
new const PreviewLayouts[][] = {"Compact", "Comfortable", "Expanded"};
new const Presets[][] = {"Default", "High contrast", "Minimal"};

// Control IDs are the one-based control positions in showcase.xaml.
enum {
    ControlScale = 3,
    ControlLayout = 5,
    ControlTest = 15,
    ControlGuide = 16,
    ControlLocal = 18,
    ControlShared = 19,
    ControlLabels = 21,
    ControlTitle = 23,
    ControlPreset = 25,
    ControlClose = 27
};

new g_fallbackMenu;

public plugin_precache() {
    cse_forms_precache_form(ShowcaseForm);
    cse_forms_precache_image("showcase-mark", true);
    cse_forms_precache_image("showcase-session", false);
}

public plugin_init() {
    register_plugin("CSE Server Forms Example", "1.0.0", "CounterStrikeExtended");
    register_clcmd("say /cse_form", "ShowExample");
    register_clcmd("say_team /cse_form", "ShowExample");

    g_fallbackMenu = menu_create("Server Forms Showcase", "HandleFallback");
    menu_additem(g_fallbackMenu, "Test action");
    menu_additem(g_fallbackMenu, "Local preview");
    menu_additem(g_fallbackMenu, "Shared preview");
    menu_additem(g_fallbackMenu, "Show guide");
    menu_additem(g_fallbackMenu, "Hide guide");
    menu_additem(g_fallbackMenu, "Preview default title");
}

public plugin_end() {
    menu_destroy(g_fallbackMenu);
}

public ShowExample(id) {
    if (!cse_forms_show(id, ShowcaseForm)) {
        menu_display(id, g_fallbackMenu);
    }
    return PLUGIN_HANDLED;
}

public HandleFallback(id, menu, item) {
    switch (item) {
        case 0: ReportAction(id, ControlTest, "", print_chat);
        case 1: ReportAction(id, ControlLocal, "1", print_chat);
        case 2: ReportAction(id, ControlShared, "1", print_chat);
        case 3: ReportAction(id, ControlGuide, "1", print_chat);
        case 4: ReportAction(id, ControlGuide, "0", print_chat);
        case 5: ReportAction(id, ControlTitle, "Server preview", print_chat);
    }
    return PLUGIN_HANDLED;
}

public cse_forms_action(player, const form[], control, const value[]) {
    if (!equal(form, ShowcaseForm)) {
        return PLUGIN_CONTINUE;
    }
    if (control == ControlClose) {
        cse_forms_hide(player);
    } else {
        ReportAction(player, control, value, print_console);
    }
    return PLUGIN_CONTINUE;
}

ReportAction(id, control, const value[], destination) {
    switch (control) {
        case ControlScale: client_print(id, destination, "[CSE Forms] Preview scale %s%%.", value);
        case ControlLayout: {
            new layout = str_to_num(value);
            if (layout >= 0 && layout < sizeof PreviewLayouts) {
                client_print(id, destination, "[CSE Forms] Layout set to %s.", PreviewLayouts[layout]);
            }
        }
        case ControlTest: client_print(id, destination, "[CSE Forms] Test action received.");
        case ControlGuide: client_print(id, destination, "[CSE Forms] Show guide set to %s.", value);
        case ControlLocal: client_print(id, destination, "[CSE Forms] Local preview selected.");
        case ControlShared: client_print(id, destination, "[CSE Forms] Shared preview selected.");
        case ControlLabels: client_print(id, destination, "[CSE Forms] Show labels set to %s.", value);
        case ControlPreset: {
            new preset = str_to_num(value);
            if (preset >= 0 && preset < sizeof Presets) {
                client_print(id, destination, "[CSE Forms] Preset set to %s.", Presets[preset]);
            }
        }
        case ControlTitle: client_print(id, destination, "[CSE Forms] Preview title set to ^"%s^".", value);
    }
}
