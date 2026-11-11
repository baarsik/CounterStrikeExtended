#!/usr/bin/env python3
"""Compile the actual native Sys_GetCDKey body and actual CRC32 routines with controlled DNS."""
from pathlib import Path
import argparse, json, subprocess, zlib
parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--evidence',type=Path,required=True)
args=parser.parse_args()
args.evidence.mkdir(parents=True,exist_ok=False)
source=args.source.read_text()
start=source.rindex('void Sys_GetCDKey(')
end=source.index('\nint Sys_InitGame(',start)
body=source[start:end]
crc=(args.source.parent/'crc.cpp').read_text()
crc=crc[crc.index('#define CRC32_INIT_VALUE'):crc.index('\n/*\n====================\nCOM_BlockSequenceCRCByte')]
prefix=r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <string>
using byte = unsigned char;
using CRC32_t = uint32_t;
constexpr int CRC32_TABLE_SIZE = 256;
constexpr int kHostNameBytes = 4096;
constexpr int kCdKeyBytes = 65;
constexpr int kCdKeyRandomMaximum = 0x7fffffff;
static int dnsCalls = 0;
static bool hostnameFails = false;
static bool hostnameTruncated = false;
static int ControlledHostname(char *output, size_t count) {
    if (hostnameFails) {
        return -1;
    }
    if (hostnameTruncated) {
        std::memset(output, 'x', count);
        return 0;
    }
    std::snprintf(output, count, "%s", "CSE-Fixture-Mac.local");
    return 0;
}
static hostent *ControlledDns(const char *) {
    ++dnsCalls;
    static unsigned char address[] = {192, 0, 2, 42};
    static char *addresses[] = {reinterpret_cast<char *>(address), nullptr};
    static hostent result{};
    result.h_addr_list = addresses;
    result.h_length = sizeof(address);
    return &result;
}
static int RandomLong(int minimum, int maximum) {
    if (minimum != 0 || maximum != kCdKeyRandomMaximum) {
        std::abort();
    }
    return 123456;
}
#define gethostname ControlledHostname
#define gethostbyname ControlledDns
#define _snprintf std::snprintf
#if defined(CSE_TEST_LEGACY)
#undef __APPLE__
#endif
'''
tests=r'''
int main(int argc, char **argv) {
    char first[kCdKeyBytes]{};
    char second[kCdKeyBytes]{};
    int length = -1;
    int dedicated = -1;
    const std::string test = argc > 1 ? argv[1] : "";
    if (test == "hostname-failure") {
        hostnameFails = true;
    }
    if (test == "hostname-truncation") {
        hostnameTruncated = true;
    }
    Sys_GetCDKey(first, &length, &dedicated);
    Sys_GetCDKey(second, nullptr, nullptr);
    if (dedicated != 0 || length != static_cast<int>(std::strlen(first)) || std::strcmp(first, second)) {
        std::fprintf(stderr, "Unstable or invalid local identity\n");
        return 1;
    }
#if defined(__APPLE__)
    if (dnsCalls != 0) {
        std::fprintf(stderr, "LAN identity called blocking DNS %d times\n", dnsCalls);
        return 2;
    }
    if (test == "hostname-failure") {
        if (std::strcmp(first, "123456")) {
            return 3;
        }
    } else {
        if (argc < 3 || std::strcmp(first, argv[2])) {
            std::fprintf(stderr, "CRC mismatch: %s\n", first);
            return 4;
        }
    }
#else
    if (dnsCalls != (hostnameFails ? 0 : 2)) {
        return 5;
    }
    if (std::strcmp(first, hostnameFails ? "123456" : "192.0.2.42")) {
        return 6;
    }
#endif
    return 0;
}
'''
unit=args.evidence/'identity.cpp'
unit.write_text(prefix+'\n'+crc+'\n'+body+'\n'+tests)
results=[]
for profile in ['macos','legacy']:
 exe=args.evidence/profile
 command=['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(unit),'-o',str(exe)]
 if profile=='legacy': command.append('-DCSE_TEST_LEGACY=1')
 subprocess.run(command,check=True)
 for case in ['hostname','hostname-failure','hostname-truncation']:
  if profile=='legacy' and case=='hostname-truncation': continue
  name='x'*4095 if case=='hostname-truncation' else 'CSE-Fixture-Mac.local'
  result=subprocess.run([str(exe),case,str(zlib.crc32(name.encode()))],capture_output=True,text=True)
  results.append({'profile':profile,'case':case,'exit':result.returncode,'stderr':result.stderr})
(args.evidence/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
assert all(row['exit']==0 for row in results)
