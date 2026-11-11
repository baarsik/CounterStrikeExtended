#!/usr/bin/env python3
"""Exercise actual SystemWrapper::DPrintf across the console format boundary."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args();a.root=a.root.resolve();a.evidence.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('extract',a.root/'tests/native/network_platform/extract_functions.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=a.root/'GoldSrc-SDK/engine/SystemWrapper.cpp'
body=m.extract(source.read_text(),'SystemWrapper::DPrintf')
prefix=r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static std::string captured;
static void Con_DPrintf(const char* format, ...) {
    // This consumer is variadic: the producer must pass text as data, never as a format.
    if (std::strcmp(format, "%s") != 0) {
        std::fputs("preformatted message forwarded as a format string\n", stderr);
        std::exit(17);
    }
    va_list args;
    va_start(args, format);
    const char* message = va_arg(args, const char*);
    captured = message;
    va_end(args);
}
class SystemWrapper {
public:
    void DPrintf(char* format, ...);
};
'''
main=r'''
int main() {
    SystemWrapper wrapper;
    char format[] = "%s %d\n";
    wrapper.DPrintf(format, "remote %s %n %08x %% chat", 48);
    assert(captured == "remote %s %n %08x %% chat 48\n");
    constexpr std::size_t DebugMessageBytes = 8192;
    std::string longMessage(DebugMessageBytes * 2, '%');
    char stringFormat[] = "%s";
    wrapper.DPrintf(stringFormat, longMessage.c_str());
    assert(captured == longMessage.substr(0, DebugMessageBytes - 1));
    return 0;
}
'''
cpp=a.evidence/'actual_logging.cpp';cpp.write_text(prefix+body+'\n'+main);exe=a.evidence/'actual_logging'
command=['clang++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(exe)]
b=subprocess.run(command,capture_output=True,text=True);(a.evidence/'build.log').write_text(b.stdout+b.stderr);assert b.returncode==0,b.stderr
r=subprocess.run([str(exe)],capture_output=True,text=True)
report={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'command':command,'exit':r.returncode,'output':r.stdout+r.stderr}
(a.evidence/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(r.returncode)
