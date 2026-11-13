#!/usr/bin/env python3
"""Exercise the production title-message initializer/parser against bundled inputs."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
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
source = (root / 'src/engine/tmessage.cpp').read_text()
functions = ['TrimSpace', 'memfgets', 'IsComment', 'IsStartOfText', 'IsEndOfText', 'IsWhiteSpace',
             'SkipSpace', 'SkipText', 'ParseFloats', 'IsToken', 'ParseDirective', 'TextMessageParse',
             'TextMessageShutdown', 'TextMessageInit']
constants = re.findall(r'^constexpr int (?:MAX_MESSAGES|NAME_HEAP_SIZE|TEXT_MESSAGE_LINE_BYTES|MSGFILE_NAME|MSGFILE_TEXT) = [^;]+;',
                       source, re.M)
assert len(constants) == 5
prefix = r'''
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <strings.h>
#include <vector>
using qboolean = int;
#include "Sequence.h"
#if __has_include("resource_text.h")
#include "resource_text.h"
#endif
#define Q_strlen std::strlen
#define Q_strncpy std::strncpy
#define Q_strcpy std::strcpy
#define Q_memcpy std::memcpy
#define Q_strnicmp strncasecmp
#define Q_atof std::atof
void Con_DPrintf(const char*, ...) {}
void Sys_Error(const char*, ...) { std::abort(); }
void* Mem_Malloc(std::size_t size) { return std::malloc(size); }
void Mem_Free(void* pointer) { std::free(pointer); }
std::vector<byte> loadedFile;
byte* COM_LoadTempFile(char*, int* size) {
    *size = static_cast<int>(loadedFile.size());
    return loadedFile.empty() ? nullptr : loadedFile.data();
}
client_textmessage_t gMessageParms{};
client_textmessage_t* gMessageTable = nullptr;
int gMessageTableCount = 0;
'''
main = r'''
int main(int argc, char** argv) {
    assert(argc == 3);
    std::ifstream input(argv[1], std::ios::binary);
    loadedFile.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    TextMessageInit();
    std::ifstream expected(argv[2], std::ios::binary);
    int count = 0;
    std::string name;
    std::string body;
    while (std::getline(expected, name, '\0')) {
        assert(std::getline(expected, body, '\0'));
        assert(count < gMessageTableCount);
        assert(name == gMessageTable[count].pName);
        assert(body == gMessageTable[count].pMessage);
        ++count;
    }
    assert(count > 0 && count == gMessageTableCount);
    TextMessageShutdown();
}
'''
cpp = args.evidence / 'actual_titles.cpp'
cpp.write_text(prefix + '\n'.join(constants) + '\n' + '\n\n'.join(extract.extract(source, name) for name in functions) + main)
exe = args.evidence / 'actual_titles'
command = ['clang++', '-std=c++17', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
           '-I', str(root / 'src/common'), str(cpp), '-o', str(exe)]
build = subprocess.run(command, text=True, capture_output=True)
(args.evidence / 'build.log').write_text(build.stdout + build.stderr)
assert build.returncode == 0, build.stderr
manifest = json.loads((root / 'src/deploy/localization/manifest.json').read_text())
results = []
for entry in manifest['files']:
    if not entry['destination'].endswith('/titles.txt'):
        continue
    text = (root / 'src/deploy/localization' / entry['source']).read_text(encoding='utf-8')
    encoding = entry['encoding']
    data = b'\xff\xfe' + text.encode('utf-16-le') if encoding == 'utf-16-le-bom' else text.encode(encoding)
    output_encoding = 'utf-8' if encoding == 'utf-16-le-bom' else encoding
    records = []
    name = None
    body = None
    for line in text.split('\n'):
        trimmed = line.strip()
        if body is not None:
            if trimmed.startswith('}'):
                records.append(name.encode(output_encoding) + b'\0' + '\n'.join(body).encode(output_encoding) + b'\0')
                body = None
            else:
                body.append(line)
        elif trimmed.startswith('{'):
            assert name is not None
            body = []
        elif trimmed and not trimmed.startswith(('//', '$')):
            name = trimmed
    assert records
    case = entry['destination'].replace('/', '-')
    path = args.evidence / case
    path.write_bytes(data)
    expected = path.with_suffix('.expected')
    expected.write_bytes(b''.join(records))
    run = subprocess.run([str(exe), str(path), str(expected)], text=True, capture_output=True)
    (args.evidence / (case + '.log')).write_text(run.stdout + run.stderr)
    results.append({'case': entry['destination'], 'messages': len(records), 'exit': run.returncode})
(args.evidence / 'result.json').write_text(json.dumps({'compile_command': command, 'results': results}, indent=2) + '\n')
print(json.dumps(results, indent=2))
raise SystemExit(any(result['exit'] for result in results))
