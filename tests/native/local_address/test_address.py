#!/usr/bin/env python3
"""Actual NET_GetLocalAddress with controlled local interfaces, socket and DNS."""
from pathlib import Path
import argparse, importlib.util, json, subprocess
parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--evidence',type=Path,required=True)
a=parser.parse_args()
a.evidence.mkdir(parents=True,exist_ok=False)
extractor=Path(__file__).resolve().parents[1]/'network_platform/extract_functions.py'
spec=importlib.util.spec_from_file_location('extract_functions',extractor)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
s=a.source.read_text()
body=module.extract(s,'NET_GetLocalAddress')
helper=module.extract(s,'NET_DefaultInterfaceIPv4') if 'NET_DefaultInterfaceIPv4(' in s else ''
prefix=r'''
#include "netadr.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <string>
#include <sys/socket.h>
constexpr int LOCAL_HOSTNAME_BYTES = 512;
constexpr int NS_SERVER = 1;
static bool noip = false;
static unsigned g_nFakeIP = 0;
static unsigned short g_arFakePorts[1]{};
static netadr_t net_local_adr{};
static int ip_sockets[3]{-1, 123, -1};
static char defaultInterface[] = "localhost";
static struct {
    char *string;
} ipname{defaultInterface};
static int dnsCalls = 0;
static int interfaceCalls = 0;
static int freed = 0;
static bool interfaceFailure = false;
static bool socketFailure = false;
static bool noUsableInterface = false;
static ifaddrs entries[7]{};
static sockaddr_in addresses[7]{};
static bool NET_ShouldUseSteamFakeIP() {
    return false;
}
static void Net_PollSteamFakeIPResult() {}
static unsigned NET_NetAdr_GetIPHostByteOrder(netadr_t *) {
    return 0;
}
static unsigned short NET_NetAdr_GetPort(netadr_t *a) {
    return a->port;
}
static void NET_NetAdr_SetIP(netadr_t *, unsigned) {}
static void NET_NetAdr_SetPort(netadr_t *, unsigned short) {}
static void Con_Printf(const char *, ...) {}
static char *NET_AdrToString(netadr_t a) {
    static char out[64];
    std::snprintf(out, sizeof(out), "%u.%u.%u.%u:%u", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
    return out;
}
static char *va(const char *f, ...) {
    static char out[256];
    va_list a;
    va_start(a, f);
    std::vsnprintf(out, sizeof(out), f, a);
    va_end(a);
    return out;
}
static void Cvar_Set(char *, const char *) {}
static void Steam_UpdateRichPresence() {}
static int ControlledHostname(char *p, size_t n) {
    std::snprintf(p, n, "fixture.invalid");
    return 0;
}
static bool NET_StringToSockaddr(char *name, sockaddr *out) {
    ++dnsCalls;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    if (inet_pton(AF_INET, name, &a.sin_addr) != 1) {
        inet_pton(AF_INET, "192.0.2.99", &a.sin_addr);
    }
    std::memcpy(out, &a, sizeof(a));
    return true;
}
static int ControlledSocket(int fd, sockaddr *out, socklen_t *size) {
    if (fd != 123) {
        std::abort();
    }
    if (socketFailure) {
        errno = EBADF;
        return -1;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(27015);
    std::memcpy(out, &a, sizeof(a));
    *size = sizeof(a);
    return 0;
}
static int ControlledInterfaces(ifaddrs **out) {
    ++interfaceCalls;
    if (interfaceFailure) {
        *out = nullptr;
        return -1;
    }
    *out = entries;
    return 0;
}
static void ControlledFree(ifaddrs *list) {
    if (list != entries) {
        std::abort();
    }
    ++freed;
}
#define gethostname ControlledHostname
#define getsockname ControlledSocket
#define getifaddrs ControlledInterfaces
#define freeifaddrs ControlledFree
#define Q_strcmp std::strcmp
#define Q_strncpy std::strncpy
#define Q_memset std::memset
#define Q_memcpy std::memcpy
'''
tests=r'''
int main(int argc, char **argv) {
    if (argc != 2) {
        return 10;
    }
    const std::string test = argv[1];
    for (int i = 0; i < 7; ++i) {
        entries[i].ifa_next = i == 6 ? nullptr : &entries[i + 1];
        entries[i].ifa_flags = IFF_UP | IFF_RUNNING;
        addresses[i].sin_family = AF_INET;
        entries[i].ifa_addr = reinterpret_cast<sockaddr *>(&addresses[i]);
        inet_pton(AF_INET, "192.0.2.1", &addresses[i].sin_addr);
    }
    entries[0].ifa_addr = nullptr;
    addresses[1].sin_family = AF_INET6;
    entries[2].ifa_flags = IFF_UP;
    entries[3].ifa_flags = IFF_RUNNING;
    entries[4].ifa_flags |= IFF_LOOPBACK;
    inet_pton(AF_INET, "192.0.2.42", &addresses[5].sin_addr);
    inet_pton(AF_INET, "192.0.2.43", &addresses[6].sin_addr);
    if (test == "explicit") {
        static char explicitIp[] = "192.0.2.8";
        ipname.string = explicitIp;
    }
    if (test == "interfaces-fail") {
        interfaceFailure = true;
    }
    if (test == "socket-fail") {
        socketFailure = true;
    }
    if (test == "no-interface") {
        for (auto &e : entries) {
            e.ifa_flags = 0;
        }
    }
    std::memset(&net_local_adr, 0xa5, sizeof(net_local_adr));
    NET_GetLocalAddress();
    if (test == "socket-fail") {
        if (!noip) {
            return 1;
        }
        return 0;
    }
    for (auto value : net_local_adr.ipx) {
        if (value != 0) {
            return 7;
        }
    }
    if (noip || net_local_adr.type != NA_IP || ntohs(net_local_adr.port) != 27015) {
        return 2;
    }
    if (test == "explicit") {
        if (dnsCalls != 1 || interfaceCalls || net_local_adr.ip[3] != 8) {
            return 3;
        }
    } else {
        if (dnsCalls) {
            std::fprintf(stderr, "Default local address performed DNS\n");
            return 4;
        }
        const unsigned expected = test == "interfaces-fail" || test == "no-interface" ? 0x7f000001U : 0xc000022aU;
        unsigned actual;
        std::memcpy(&actual, net_local_adr.ip, sizeof(actual));
        if (ntohl(actual) != expected) {
            std::fprintf(stderr, "Wrong IPv4: %s\n", NET_AdrToString(net_local_adr));
            return 5;
        }
        if (interfaceCalls != 1 || freed != (interfaceFailure ? 0 : 1)) {
            return 6;
        }
    }
    return 0;
}
'''
unit=a.evidence/'address.cpp'
unit.write_text(prefix+'\n'+module.extract(s,'SockadrToNetadr')+'\n'+helper+'\n'+body+'\n'+tests)
exe=a.evidence/'address'
subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(a.source.parent.parent/'common'),str(unit),'-o',str(exe)],check=True)
results=[]
for case in ['default','explicit','interfaces-fail','no-interface','socket-fail']:
 r=subprocess.run([str(exe),case],capture_output=True,text=True)
 results.append({'case':case,'exit':r.returncode,'stderr':r.stderr})
(a.evidence/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2));assert all(r['exit']==0 for r in results)
