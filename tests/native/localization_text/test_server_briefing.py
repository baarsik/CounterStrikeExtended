#!/usr/bin/env python3
"""Run the actual CS world-spawn briefing path with controlled engine services."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
args.evidence.mkdir(parents=True, exist_ok=False)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
source = (root / 'src/cstrike/dlls/world.cpp').read_text(encoding='cp1252')
functions = ['SetMapBriefingText', 'CWorld::Spawn'] if 'void SetMapBriefingText(' in source else ['CWorld::Spawn']
prefix = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "resource_text.h"
char g_szMapBriefingText[512]{};
bool g_fGameOver = true;
constexpr bool FALSE = false;
struct Globals { const char* mapname = "sample"; } globals;
Globals* gpGlobals = &globals;
class CWorld {
public:
    bool m_worldMapTeamsKnown = false;
    void Precache() {}
    void Spawn();
};
void EmptyEntityHashTable() {}
#define STRING(value) (value)
const char* UTIL_VarArgs(const char* format, const char* name = nullptr) {
    static char path[260];
    std::snprintf(path, sizeof(path), format, name);
    return path;
}
std::string bytes;
bool fallback = false;
int frees = 0;
int precaches = 0;
void* Load(const char* path, int* length) {
    if (fallback && std::string(path) != "maps/default.txt") { *length = 0; return nullptr; }
    *length = static_cast<int>(bytes.size());
    return const_cast<char*>(bytes.c_str());
}
void Free(void* data) { if (data) { assert(data == bytes.c_str()); ++frees; } }
void Precache(const char*) { ++precaches; }
#define LOAD_FILE_FOR_ME Load
#define FREE_FILE Free
#define PRECACHE_GENERIC ::Precache
'''
main = r'''
int main(int argc, char** argv) {
    assert(argc == 4);
    std::ifstream input(argv[1], std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    std::ifstream expectedFile(argv[2], std::ios::binary);
    std::string expected{std::istreambuf_iterator<char>(expectedFile), std::istreambuf_iterator<char>()};
    fallback = std::string(argv[3]) == "fallback";
    CWorld world;
    world.Spawn();
    assert(std::string(g_szMapBriefingText) == expected.substr(0, sizeof(g_szMapBriefingText) - 2));
    assert(world.m_worldMapTeamsKnown && !g_fGameOver && frees == 1 && precaches == 1);
}
'''
cpp = args.evidence / 'actual_briefing.cpp'
cpp.write_text(prefix + '\n\n'.join(extract.extract(source, name) for name in functions) + main)
exe = args.evidence / 'actual_briefing'
command = ['clang++', '-std=c++17', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
           '-I', str(root / 'src/common'), str(cpp), '-o', str(exe)]
build = subprocess.run(command, text=True, capture_output=True)
(args.evidence / 'build.log').write_text(build.stdout + build.stderr)
assert build.returncode == 0, build.stderr
results = []
for language in ('french', 'german', 'italian', 'spanish', 'schinese', 'tchinese', 'koreana'):
    text = (root / 'src/deploy/localization' / ('cstrike_' + language) / 'maps/de_dust.txt').read_text(encoding='utf-8')
    data = args.evidence / (language + '.txt')
    data.write_bytes(b'\xff\xfe' + text.encode('utf-16-le'))
    expected = args.evidence / (language + '.expected')
    expected.write_bytes(text.encode('utf-8'))
    for mode in ('primary', 'fallback'):
        run = subprocess.run([str(exe), str(data), str(expected), mode], text=True, capture_output=True)
        (args.evidence / (language + '-' + mode + '.log')).write_text(run.stdout + run.stderr)
        results.append({'language': language, 'mode': mode, 'exit': run.returncode})
(args.evidence / 'result.json').write_text(json.dumps({'compile_command': command, 'results': results}, indent=2) + '\n')
print(json.dumps(results, indent=2))
raise SystemExit(any(result['exit'] for result in results))
