"""Extract the owning argument parsers verbatim; no replacement parsing algorithm."""
import pathlib
import re


def definitions(root: pathlib.Path) -> str:
    sdk = root / 'GoldSrc-SDK/engine'
    limits = (sdk / 'common.h').read_text()
    common = (sdk / 'common.cpp').read_text()
    system = (sdk / 'sys_dll2.cpp').read_text()
    defines = '\n'.join(re.findall(r'^#define (?:MAX_NUM_ARGVS|CMDLINE_LENGTH) .*$', limits, re.M))
    storage = common[common.index('static constexpr int NUM_SAFE_ARGVS'):common.index('static constexpr int STRING_NOT_EQUAL')]
    common_body = common[common.index('void COM_InitArgv('):common.index('\nvoid Q_strcpy(')]
    system_body = system[system.index('void Sys_InitArgv('):system.index('\nvoid Sys_InitMemory(')]
    return '#include <cstring>\n' + defines + '''
using qboolean = int;
static char com_cmdline[CMDLINE_LENGTH];
static int com_argc;
static char** com_argv;
static char* argv[MAX_NUM_ARGVS];
static const char com_empty_string[] = "";
static struct {
    int argc;
    char** argv;
} host_parms;
static int Q_strcmp(const char* first, const char* second) {
    return std::strcmp(first, second);
}
''' + storage + common_body + system_body
