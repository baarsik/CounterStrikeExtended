#include <amxmodx>
#include <cse_extstats>

public plugin_init() {
    register_plugin("CSE Extended Stats Example", "1.0.0", "CounterStrikeExtended");
    register_clcmd("say /cse_stats", "ShowExample");
}

public ShowExample(id) {
    if (is_user_alive(id)) {
        cse_set_user_health(id, 99999);
        cse_set_user_armor(id, 99999);
        client_print(id, print_chat, "CSE identified: %d", cse_is_supported(id));
    }
    return PLUGIN_HANDLED;
}
