#include <amxmodx>
#include <fakemeta>

new const ProbeSound[] = "cse_fixture/probe.wav";
new const ProbeResource[] = "cse_fixture/probe.txt";
new ProbeMessage;

public plugin_precache() {
    precache_sound(ProbeSound);
    precache_generic(ProbeResource);
    force_unmodified(force_exactfile, {0, 0, 0}, {0, 0, 0}, "models/w_c4.mdl");
}

public plugin_init() {
    register_plugin("CSE external compatibility fixture", "1.0.0", "CounterStrikeExtended");
    register_cvar("cse_fixture_version", "1", FCVAR_SERVER);
    register_clcmd("cse_fixture_start", "ProbeStart");
    register_clcmd("cse_fixture_ack", "ProbeAck");
    ProbeMessage = engfunc(EngFunc_RegUserMsg, "CSEProbe", -1);
    log_amx("CSE_FIXTURE_READY custom_message=%d", ProbeMessage);
}

public client_authorized(id, const authid[]) {
    new authenticated = contain(authid, "STEAM_0:") == 0 || contain(authid, "STEAM_1:") == 0;
    log_amx("CSE_FIXTURE_AUTH id=%d steam_authorized=%d", id, authenticated);
}

public ProbeStart(id) {
    if (!is_user_connected(id)) {
        return PLUGIN_HANDLED;
    }
    client_print(id, print_chat, "CSE_FIXTURE_CHAT_READY");
    set_hudmessage(0, 255, 0, -1.0, 0.3, 0, 0.0, 8.0, 0.0, 0.0);
    show_hudmessage(id, "CSE_FIXTURE_HUD_READY");
    new menu = menu_create("CSE_FIXTURE_MENU_READY", "ProbeMenuReply");
    menu_additem(menu, "Acknowledge fixture menu", "ack");
    menu_display(id, menu, 0);
    query_client_cvar(id, "cl_updaterate", "ProbeCvarReply");
    emit_sound(id, CHAN_ITEM, ProbeSound, VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
    if (ProbeMessage > 0) {
        // Unknown custom messages must be consumed without corrupting the next
        // message. This fixture does not install a client-side message hook.
        // Fakemeta registers directly with the engine, outside Metamod's
        // user-message name cache used by AMXX message_begin validation.
        engfunc(EngFunc_MessageBegin, MSG_ONE, ProbeMessage, Float:{0.0, 0.0, 0.0}, id);
        write_byte(0x5a);
        write_short(0x7fff);
        write_long(0x12345678);
        write_string("CSE custom message boundary");
        message_end();
    }
    log_amx("CSE_FIXTURE_STARTED id=%d", id);
    return PLUGIN_HANDLED;
}

public ProbeMenuReply(id, menu, item) {
    log_amx("CSE_FIXTURE_MENU_REPLY id=%d item=%d", id, item);
    menu_destroy(menu);
    return PLUGIN_HANDLED;
}

public ProbeCvarReply(id, const name[], const value[]) {
    log_amx("CSE_FIXTURE_CVAR_REPLY id=%d name=%s value=%s", id, name, value);
}

public ProbeAck(id) {
    log_amx("CSE_FIXTURE_CLIENT_ACK id=%d", id);
    client_print(id, print_chat, "CSE_FIXTURE_ACK_RECEIVED");
    return PLUGIN_HANDLED;
}
