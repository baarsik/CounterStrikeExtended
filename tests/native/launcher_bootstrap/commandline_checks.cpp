#include "common/icommandline.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unistd.h>

extern ICommandLine* cmdline;
extern bool RestoreMacLauncherCommandLine(const char* commandLine);

int main() {
    const char* original = "\"/owned path/-w executable\" -windowed -width 800 -w 640 -h 480 "
        "-height 600 -heapsize 65536 -name \"quoted -w value\" -literal @already-expanded "
        "-w 1024 -h 768 -extra \"kept +load text\"";
    assert(RestoreMacLauncherCommandLine(original));
    cmdline->RemoveParm("-w");
    cmdline->RemoveParm("-h");
    const char* expected = "\"/owned path/-w executable\" -windowed -width 800 "
        "-height 600 -heapsize 65536 -name \"quoted -w value\" -literal @already-expanded "
        "-extra \"kept +load text\"";
    assert(std::strcmp(cmdline->GetCmdLine(), expected) == 0);
    cmdline->RemoveParm("-width");
    cmdline->RemoveParm("-height");
    assert(std::strcmp(cmdline->GetCmdLine(), "\"/owned path/-w executable\" -windowed "
        "-heapsize 65536 -name \"quoted -w value\" -literal @already-expanded "
        "-extra \"kept +load text\"") == 0);
    cmdline->RemoveParm("+load");
    assert(std::strstr(cmdline->GetCmdLine(), "\"kept +load text\"") != nullptr);
    cmdline->RemoveParm("-does-not-exist");
    assert(std::strstr(cmdline->GetCmdLine(), "-literal @already-expanded") != nullptr);
    assert(RestoreMacLauncherCommandLine("hl -w 1 -w 2 -w 3"));
    cmdline->RemoveParm("-w");
    assert(std::strcmp(cmdline->GetCmdLine(), "hl") == 0);
    // Expand an actual response file once, then preserve that canonical text
    // after its file is gone. Restart removal must never re-expand the line.
    char responsePath[] = "/tmp/cse-restart-response-XXXXXX";
    const int descriptor = mkstemp(responsePath);
    assert(descriptor >= 0);
    FILE* response = fdopen(descriptor, "w");
    assert(response != nullptr);
    std::fputs("-windowed -width 800 -w 640 -name \"response -w value\"", response);
    std::fclose(response);
    const std::string responseCommand = std::string("hl -h 480 -height 600 @") + responsePath;
    cmdline->CreateCmdLine(responseCommand.c_str());
    unlink(responsePath);
    cmdline->RemoveParm("-w");
    cmdline->RemoveParm("-h");
    assert(std::strstr(cmdline->GetCmdLine(), "-windowed -width 800") != nullptr);
    assert(std::strstr(cmdline->GetCmdLine(), "-name \"response -w value\"") != nullptr);
    assert(std::strstr(cmdline->GetCmdLine(), "-height 600") != nullptr);
    assert(std::strstr(cmdline->GetCmdLine(), responsePath) == nullptr);
    std::puts("actual Apple RemoveParm: exact collisions, repeated options, quoted spans, canonical text preserved");
}
