#include "actual_engine.inc"

static void Register(cvar_t* variable) {
    variable->next = cvar_vars;
    cvar_vars = variable;
}

static void SetFilter(int level) {
    static char levels[][2] = {"0", "1", "2"};
    cl_filterstuffcmd.value = static_cast<float>(level);
    cl_filterstuffcmd.string = levels[level];
}

static bool Execute(const std::string& text, bool privileged, bool validate = true) {
    values.clear();
    executed.clear();
    cmd_text.cursize = 0;
    cmd_text_untrusted.cursize = 0;
    if (validate && !privileged && !ValidStuffText(text.c_str())) {
        return false;
    }
    Cbuf_AddTextToUntrustedBuffer(text.c_str());
    Cbuf_ExecuteCommandsFromBuffer(&cmd_text_untrusted, privileged, -1);
    return !executed.empty() || !values.empty();
}

static void Require(bool actual, bool expected, const std::string& input, int level) {
    if (actual != expected) {
        std::fprintf(stderr, "level %d: %s unexpectedly %s\n", level, input.c_str(), actual ? "allowed" : "blocked");
        std::exit(1);
    }
}

int main() {
    // A newly initialized client must get Default protection before reading a saved configuration.
    cl_filterstuffcmd.value = Q_atof(cl_filterstuffcmd.string);
    Register(&cl_filterstuffcmd);
    cvar_t settings[] = {
        {const_cast<char*>("name"), const_cast<char*>("player"), FCVAR_ARCHIVE, 0, nullptr},
        {const_cast<char*>("fps_override"), const_cast<char*>("0"), FCVAR_ARCHIVE, 0, nullptr},
        {const_cast<char*>("gamma"), const_cast<char*>("2.2"), FCVAR_ARCHIVE, 0, nullptr},
        {const_cast<char*>("brightness"), const_cast<char*>("0"), FCVAR_ARCHIVE, 0, nullptr},
        {const_cast<char*>("fps_max"), const_cast<char*>("100"), FCVAR_FILTERSTUFFTEXT, 0, nullptr}
    };
    for (cvar_t& setting : settings) {
        Register(&setting);
    }
    cmd_function_t commands[] = {
        {"snapshot", 0, ObserveCommand, nullptr},
        {"screenshot", 0, ObserveCommand, nullptr},
        {"hideconsole", 0, ObserveCommand, nullptr},
        {"echo", 0, ObserveCommand, nullptr},
        {"connect", CMD_FLAG_PRIVILEGED, ObserveCommand, nullptr}
    };
    for (std::size_t index = 0; index + 1 < std::size(commands); ++index) {
        commands[index].next = &commands[index + 1];
    }
    cmd_functions = commands;
    Require(Execute("fps_max 75", false), false, "initial default fps_max protection", 1);

    const char* extended[] = {"snapshot", "screenshot", "hideconsole", "name", "fps_override", "gamma", "brightness"};
    for (int level : {0, 1, 2}) {
        SetFilter(level);
        Require(Execute("fps_max 75", false), level == 0, "fps_max 75", level);
        Require(Execute("echo harmless", false), true, "echo harmless", level);
        Require(Execute("echo gamma", false), true, "echo gamma argument", level);
        for (const char* name : extended) {
            const std::string payload = std::string(name) + " 1";
            Require(Execute(payload, false), level < 2, payload, level);
            Require(Execute(payload, false, false), level < 2, "direct " + payload, level);
            Require(Execute(payload, true), true, "trusted " + payload, level);
            std::string mixed = payload;
            for (std::size_t index = 0; index < std::strlen(name); index += 2) {
                mixed[index] = static_cast<char>(std::toupper(mixed[index]));
            }
            Require(Execute(mixed, false), level < 2, mixed, level);
            Require(Execute("\"" + std::string(name) + "\" 1", false), level < 2, "quoted " + payload, level);
            cmdalias_t alias{"filter_test_alias", payload.c_str(), nullptr};
            cmd_alias = &alias;
            Require(Execute("filter_test_alias", false), level < 2, "alias " + payload, level);
            cmd_alias = nullptr;
            values.clear();
            executed.clear();
            cmd_text_untrusted.cursize = 0;
            const std::string first = payload.substr(0, 2);
            const std::string second = payload.substr(2) + "\n";
            Cbuf_AddTextToUntrustedBuffer(first.c_str());
            Cbuf_AddTextToUntrustedBuffer(second.c_str());
            Cbuf_ExecuteCommandsFromBuffer(&cmd_text_untrusted, FALSE, -1);
            Require(!executed.empty() || !values.empty(), level < 2, "split " + payload, level);
        }
        constexpr char ConnectionCommand[] = "connect";
        constexpr unsigned ConnectionCommandLength = sizeof(ConnectionCommand) - 1;
        for (unsigned mask = 0; mask < (1u << ConnectionCommandLength); ++mask) {
            std::string command = ConnectionCommand;
            for (unsigned index = 0; index < command.size(); ++index) {
                if (mask & (1u << index)) {
                    command[index] = static_cast<char>(std::toupper(command[index]));
                }
            }
            Require(Execute(command + " 127.0.0.1:27015", false), false, command, level);
        }
    }
    // The command-name search must consider every match and recognize tokenizer boundaries.
    Require(AllowedCommand("unrestricted; restricted", "restricted"), false, "second command match", 0);
    Require(AllowedCommand("echo;\t\"restricted\"", "restricted"), false, "quoted command", 0);
    Require(AllowedCommand("unrestricted", "restricted"), true, "partial command name", 0);
    Require(ValidStuffText("reconnect"), true, "live reconnect", 2);
    Require(ValidStuffText("echo \"unfinished"), true, "quoted argument ending at the terminator", 2);

    std::puts("PASS engine filter levels, default, trusted commands, mixed case, quotes, aliases and split buffers");
}
