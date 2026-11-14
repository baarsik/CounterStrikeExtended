#include <amxmodx>
#include <fakemeta>
#include <cstrike>
#include <hamsandwich>
#include <cse_extstats>

new const Values[] = {0, 1, 255, 256, 511, 512, 999, 1000, 32767, 32768, 65535, 65536, 99999, 100000};
new Subject;
new Step;

public plugin_init() {
    register_plugin("CSE extended stats qualification", "1.0", "CSE");
    new health = get_user_msgid("HealthEx");
    new armor = get_user_msgid("BatteryEx");
    server_print("CSE_PROBE init health_id=%d armor_id=%d invalid_identity=%d", health, armor, cse_is_supported(0));
    if (health && armor) {
        register_message(health, "OnHealth");
        register_message(armor, "OnArmor");
    }
    register_srvcmd("cse_probe_run", "Run");
}

public OnHealth(message, destination, recipient) {
    LogWire("HealthEx", destination, recipient);
}

public OnArmor(message, destination, recipient) {
    LogWire("BatteryEx", destination, recipient);
}

LogWire(const name[], destination, recipient) {
    if (get_msg_args() != 2) {
        server_print("CSE_PROBE malformed name=%s args=%d", name, get_msg_args());
        return;
    }
    server_print("CSE_PROBE wire name=%s dest=%d recipient=%d subject=%d amount=%d time=%.3f",
        name, destination, recipient, get_msg_arg_int(1), get_msg_arg_int(2), get_gametime());
}

public Run() {
    remove_task(7319);
    new argument[16];
    read_argv(1, argument, charsmax(argument));
    Subject = str_to_num(argument);
    if (!Subject) {
        Subject = engfunc(EngFunc_CreateFakeClient, "CSEProbe");
        if (!Subject) {
            server_print("CSE_PROBE error create_bot");
            return;
        }
        new reject[128];
        dllfunc(DLLFunc_ClientConnect, Subject, "CSEProbe", "127.0.0.1", reject);
        dllfunc(DLLFunc_ClientPutInServer, Subject);
        set_pev(Subject, pev_flags, pev(Subject, pev_flags) | FL_FAKECLIENT);
        cs_set_user_team(Subject, CS_TEAM_T);
        engclient_cmd(Subject, "jointeam", "1");
        engclient_cmd(Subject, "joinclass", "1");
    }
    if (!is_user_connected(Subject)) {
        server_print("CSE_PROBE error disconnected_subject");
        return;
    }
    Step = 0;
    ExecuteHamB(Ham_CS_RoundRespawn, Subject);
    set_task(1.5, "Next", 7319);
}

public Next() {
    if (Step == sizeof(Values)) {
        server_print("CSE_PROBE complete subject=%d", Subject);
        return;
    }
    if (!is_user_alive(Subject)) {
        ExecuteHamB(Ham_CS_RoundRespawn, Subject);
    }
    new amount = Values[Step++];
    cse_set_user_armor(Subject, amount);
    cse_set_user_health(Subject, amount);
    new Float:health;
    new Float:armor;
    pev(Subject, pev_health, health);
    pev(Subject, pev_armorvalue, armor);
    server_print("CSE_PROBE case subject=%d expected=%d health=%d armor=%d alive=%d",
        Subject, amount, floatround(health), floatround(armor), is_user_alive(Subject));
    set_task(0.5, "Next", 7319);
}
