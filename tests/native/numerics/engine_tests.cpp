#include "engine_boundary.h"

#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if !defined(_WIN32)
#define _snprintf snprintf
#endif

std::string variableName;
std::string variableValue;
std::string consoleText;
std::string fileText;
std::uint32_t angleValue;
int angleBits;
bool redirectValue;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Engine numeric fixture failed: %s\n", message);
        std::exit(1);
    }
}

void HookSetValue(char** name, float* value) {
    Require(name && *name && value, "cvar hook arguments");
    if (redirectValue) {
        *value = 1.5f;
    }
}

NumericHookTable g_engdstAddrs{HookSetValue};

void Cvar_Set(const char* name, const char* value) {
    variableName = name;
    variableValue = value;
}

void Q_strcat(char* target, const char* suffix) {
    std::strcat(target, suffix);
}

void Con_Printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    Require(std::strcmp(format, "%s") == 0, "console formatting contract");
    consoleText = va_arg(args, const char*);
    va_end(args);
}

int FS_FPrintf(FileHandle_t file, const char* format, ...) {
    Require(file != nullptr && std::strcmp(format, "%s") == 0, "file formatting contract");
    va_list args;
    va_start(args, format);
    fileText = va_arg(args, const char*);
    va_end(args);
    return static_cast<int>(fileText.size());
}

void MSG_WriteBits(unsigned int data, int numBits) {
    angleValue = data;
    angleBits = numBits;
}

struct InvalidAngleWidth {};
[[noreturn]] void Sys_Error(const char*) {
    throw InvalidAngleWidth{};
}

namespace {
void WriteInt(std::FILE* output, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        Require(std::fputc(static_cast<unsigned char>(value >> shift), output) != EOF, "write engine corpus");
    }
}

void WriteText(std::FILE* output, const std::string& value) {
    WriteInt(output, static_cast<std::uint32_t>(value.size()));
    Require(std::fwrite(value.data(), 1, value.size(), output) == value.size(), "write string corpus");
}

void CheckAngle(std::FILE* output, float angle) {
    for (int width = 0; width < 32; ++width) {
        angleBits = -1;
        MSG_WriteBitAngle(angle, width);
        Require(angleBits == width, "angle write width");
        Require(angleValue < (1u << width), "angle value fits encoded width");
        WriteInt(output, angleValue);
    }
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "one engine corpus path required");
    std::FILE* output = std::fopen(argv[1], "wb");
    Require(output != nullptr, "open engine corpus");
    constexpr float Angles[] = {0.0f, -0.0f, 0.125f, -0.125f, 45.0f, 90.0f, 180.0f, 359.999f,
        -359.999f, 360.0f, -360.0f, 720.125f, -720.125f, 10000000.0f, -10000000.0f};
    for (const float angle : Angles) {
        CheckAngle(output, angle);
    }
    std::uint32_t state = 0x6d2b79f5;
    constexpr int AngleSamples = 10000;
    for (int index = 0; index < AngleSamples; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        float angle;
        std::memcpy(&angle, &state, sizeof(angle));
        CheckAngle(output, angle);
    }
    bool rejected = false;
    try {
        MSG_WriteBitAngle(1.0f, 32);
    } catch (const InvalidAngleWidth&) {
        rejected = true;
    }
    Require(rejected, "invalid bit width rejected before shifting");
    char name[] = "fixture_cvar";
    char text[] = "unused";
    constexpr float Values[] = {0.0f, -0.0f, 1.0f, -1.0f, 1.25f, -1.25f, 0.0000001f,
        -0.0000001f, 2147483520.0f, 2147483648.0f, -2147483648.0f, -2147483904.0f};
    for (float value : Values) {
        Cvar_SetValue(name, value);
        Require(variableName == name && !variableValue.empty(), "cvar name and value capture");
        WriteText(output, variableValue);
        for (int flags = 0; flags < 8; ++flags) {
            cvar_t variable{name, text, flags, value, nullptr};
            consoleText.clear();
            fileText.clear();
            Cmd_CvarListPrintCvar(&variable, &variable);
            Require(consoleText == fileText && consoleText.back() == '\n', "console/file cvar list parity");
            WriteText(output, consoleText);
            fileText.clear();
            Cmd_CvarListPrintCvar(&variable, nullptr);
            Require(fileText.empty(), "absent file does not write");
        }
    }
    Cvar_SetValue(name, -1.25f);
    Require(variableValue == "-1.250000", "fraction formatting");
    Cvar_SetValue(name, 12.0f);
    Require(variableValue == "12", "integer formatting");
    redirectValue = true;
    Cvar_SetValue(name, 12.0f);
    Require(variableValue == "1.500000", "hook can modify value before conversion");
    Require(std::fclose(output) == 0, "close engine corpus");
    std::puts("320,480 angle writes and 96 cvar flag combinations checked");
    return 0;
}
