namespace CSE_MEMORY_FIXTURE_OWNER {
int COM_CheckParm(const char* argument) {
    services.trace += std::strcmp(argument, "-heapsize") == 0 ? 'H' : 'M';
    return ActualCheckParm(argument);
}

void Sys_Error(const char* message, ...) {
    services.trace += 'E';
    services.error = message;
    va_list arguments;
    va_start(arguments, message);
    if (std::strcmp(message, "Available memory less than 15MB!!! %i\n") == 0) {
        services.errorBytes = va_arg(arguments, int);
    } else if (std::strcmp(message, "Unable to allocate %.2f MB\n") == 0) {
        services.errorMebibytes = va_arg(arguments, double);
    }
    va_end(arguments);
    if (!services.returnFromError) {
        throw std::runtime_error(message);
    }
}

#if CSE_MEMORY_FIXTURE_SW
#include "sw-Sys_InitMemory.inc"
#include "sw-Sys_ShutdownMemory.inc"
#else
#include "hw-Sys_InitMemory.inc"
#include "hw-Sys_ShutdownMemory.inc"
#endif

struct Adapter {
    static void Set(int initial, bool dedicated, const std::vector<std::string>& input) {
        Check(host_parms.membase == nullptr, "previous owned arena released before next case");
        host_parms = {};
        host_parms.memsize = initial;
        g_bIsDedicatedServer = dedicated;
        static std::vector<std::string> text;
        static std::vector<char*> arguments;
        text = input;
        arguments.clear();
        for (std::string& item : text) {
            arguments.push_back(&item[0]);
        }
        com_argc = static_cast<int>(arguments.size());
        com_argv = arguments.data();
    }
    static quakeparms_t& Heap() {
        return host_parms;
    }
    static void Init() {
        Sys_InitMemory();
    }
    static void Shutdown() {
        Sys_ShutdownMemory();
    }
};
}
