#include "icommandline.h"

#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t CommandPayloadBytes = 4095;

bool Check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "launcher command-line failure: %s\n", label);
    }
    return condition;
}

bool Line(const std::string& expected, const char* label) {
    const char* actual = cmdline->GetCmdLine();
    std::uint64_t hash = UINT64_C(14695981039346656037);
    if (actual) {
        for (const unsigned char* byte = reinterpret_cast<const unsigned char*>(actual); *byte; ++byte) {
            hash = (hash ^ *byte) * UINT64_C(1099511628211);
        }
    }
    std::printf("%s bytes=%zu hash=%016llx\n", label, actual ? std::strlen(actual) : 0,
        static_cast<unsigned long long>(hash));
    return Check(actual && expected == actual, label);
}

bool Value(const char* parameter, const char* expected) {
    char* value = nullptr;
    const char* found = cmdline->CheckParm(parameter, &value);
    std::printf("value %s=%s\n", parameter, value ? value : "<null>");
    return Check(found && value && std::strcmp(value, expected) == 0, parameter);
}

bool WriteResponse(const std::string& bytes) {
    FILE* file = std::fopen("response.params", "wb");
    if (!file) {
        return false;
    }
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    return std::fclose(file) == 0 && written == bytes.size();
}

bool Basic() {
    if (!Check(cmdline->GetCmdLine() == nullptr, "initial state")) {
        return false;
    }
    cmdline->AppendParm("-first", "alpha");
    bool passed = Line("-first alpha", "initial append");
    cmdline->CreateCmdLine("hl -width 640 -name player +map de_dust2");
    passed = Line("hl -width 640 -name player +map de_dust2", "raw command") && passed;
    passed = Value("-width", "640") && Value("-name", "player") && passed;
    passed = Check(cmdline->CheckParm("-missing", nullptr) == nullptr, "missing parameter") && passed;
    cmdline->CreateCmdLine("");
    return Line("", "empty command") && passed;
}

bool Mutators() {
    cmdline->CreateCmdLine("hl -width 640 -name old +map de_dust2");
    cmdline->RemoveParm("-width");
    bool passed = Line("hl -name old +map de_dust2", "remove middle");
    cmdline->AppendParm("-width", "800");
    passed = Line("hl -name old +map de_dust2 -width 800", "append") && passed;
    cmdline->SetParm("-name", "player");
    passed = Line("hl +map de_dust2 -width 800 -name player", "set string") && passed;
    cmdline->SetParm("-width", 1024);
    passed = Line("hl +map de_dust2 -name player -width 1024", "set integer") && passed;
    cmdline->RemoveParm("-width");
    passed = Line("hl +map de_dust2 -name player", "remove last") && passed;
    cmdline->RemoveParm("");
    cmdline->RemoveParm(nullptr);
    return Line("hl +map de_dust2 -name player", "empty removal") && passed;
}

bool Quoted() {
    cmdline->CreateCmdLine("hl -title \"hello world\" -path \"a\\b\" +map de_dust2");
    bool passed = Line("hl -title \"hello world\" -path \"a\\b\" +map de_dust2", "quoted raw bytes");
    // The retained parser extracts until a literal space; it does not unquote values.
    passed = Value("-title", "\"hello") && Value("-path", "\"a\\b\"") && passed;
    cmdline->SetParm("-title", "\"two words\"");
    return Value("-title", "\"two") && passed;
}

bool Response() {
    if (!WriteResponse("-loaded yes\n+map de_dust2")) {
        return false;
    }
    cmdline->CreateCmdLine("hl @response.params");
    // Preserve the legacy filename cursor behavior as well as newline conversion.
    bool passed = Line("hl -loaded yes +map de_dust2 esponse.params", "response expansion");
    passed = Value("-loaded", "yes") && passed;
    cmdline->CreateCmdLine("hl prefix@response.params");
    passed = Line("hl prefix@response.params", "embedded response marker") && passed;
    std::remove("response.params");
    cmdline->CreateCmdLine("hl @missing.params");
    return Line("hl issing.params", "missing response legacy cursor") && passed;
}

bool Whitespace() {
    if (!WriteResponse("-loaded yes")) {
        return false;
    }
    bool passed = true;
#if defined(_WIN32)
    // Preserve the Windows signed-char ctype path; do not feed it invalid negative ctype inputs.
    constexpr unsigned int LastByte = 127;
#else
    constexpr unsigned int LastByte = 255;
#endif
    for (unsigned int byte = 1; byte <= LastByte; ++byte) {
        std::string input = "h";
        input += static_cast<char>(byte);
        input += "@response.params";
        cmdline->CreateCmdLine(input.c_str());
        const bool whitespace = byte == ' ' || byte == '\t' || byte == '\n' ||
            byte == '\r' || byte == '\v' || byte == '\f';
        std::string expected = "h";
        expected += static_cast<char>(byte);
        expected += whitespace ? "-loaded yes esponse.params" : "@response.params";
        if (!Line(expected, "whitespace byte")) {
            std::fprintf(stderr, "byte=%u\n", byte);
            passed = false;
            break;
        }
    }
    std::remove("response.params");
    return passed;
}

bool Limits() {
    const std::string longCommand(CommandPayloadBytes + 100, 'x');
    cmdline->CreateCmdLine(longCommand.c_str());
    bool passed = Line(std::string(CommandPayloadBytes, 'x'), "raw 4096-byte buffer");
    if (!WriteResponse(std::string(CommandPayloadBytes + 100, 'z'))) {
        return false;
    }
    cmdline->CreateCmdLine("@response.params");
    passed = Line(std::string(CommandPayloadBytes - 1, 'z') + " ", "response 4096-byte buffer") && passed;
    std::remove("response.params");
    return passed;
}

#if !defined(_WIN32)
bool Argv() {
    const char* arguments[] = {"hl", "-name", "two words", "-tabs", "a\tb", "", "a\"b", "+map", "de_dust2"};
    cmdline->CreateCmdLine(static_cast<int>(sizeof(arguments) / sizeof(arguments[0])), arguments);
    bool passed = Line("hl -name \"two words\" -tabs a\tb  a\"b +map de_dust2", "argv legacy encoding");
    cmdline->CreateCmdLine(0, nullptr);
    return Line("", "zero argc") && passed;
}

bool ArgvBoundary() {
    const std::string exact(CommandPayloadBytes, 'x');
    const char* one[] = {exact.c_str()};
    cmdline->CreateCmdLine(1, one);
    bool passed = Line(exact, "argv exact unquoted boundary");
    constexpr std::size_t ExecutableAndSeparatorBytes = 3;
    constexpr std::size_t QuotesBytes = 2;
    std::string quoted(CommandPayloadBytes - ExecutableAndSeparatorBytes - QuotesBytes, 'q');
    quoted[1] = ' ';
    const char* two[] = {"hl", quoted.c_str()};
    cmdline->CreateCmdLine(2, two);
    return Line("hl \"" + quoted + "\"", "argv exact quoted boundary") && passed;
}

bool ArgvOverflow() {
    const std::string unquoted(CommandPayloadBytes + 1, 'x');
    const std::string quoted = " " + std::string(CommandPayloadBytes - 1, 'q');
    const char* one[] = {unquoted.c_str()};
    const char* two[] = {quoted.c_str()};
    for (const char** arguments : {one, two}) {
        cmdline->CreateCmdLine("hl -keep yes");
        bool rejected = false;
        try {
            cmdline->CreateCmdLine(1, arguments);
        } catch (const std::length_error& error) {
            rejected = std::strcmp(error.what(), "Command line exceeds 4095 bytes") == 0;
        }
        if (!Check(rejected, "argv overflow length_error") || !Line("hl -keep yes", "overflow preserves state")) {
            return false;
        }
    }
    std::vector<const char*> accumulated(CommandPayloadBytes / 2 + 2, "x");
    cmdline->CreateCmdLine("hl -keep yes");
    bool rejected = false;
    try {
        cmdline->CreateCmdLine(static_cast<int>(accumulated.size()), accumulated.data());
    } catch (const std::length_error& error) {
        rejected = std::strcmp(error.what(), "Command line exceeds 4095 bytes") == 0;
    }
    return Check(rejected, "accumulated argv overflow") && Line("hl -keep yes", "accumulated overflow state");
}
#endif
}

int main(int argc, char** argv) {
    if (argc != 2 || !std::setlocale(LC_CTYPE, "C")) {
        return 2;
    }
    bool passed = false;
    if (std::strcmp(argv[1], "basic") == 0) {
        passed = Basic();
    }
    if (std::strcmp(argv[1], "mutators") == 0) {
        passed = Mutators();
    }
    if (std::strcmp(argv[1], "quoted") == 0) {
        passed = Quoted();
    }
    if (std::strcmp(argv[1], "response") == 0) {
        passed = Response();
    }
    if (std::strcmp(argv[1], "whitespace") == 0) {
        passed = Whitespace();
    }
    if (std::strcmp(argv[1], "limits") == 0) {
        passed = Limits();
    }
#if !defined(_WIN32)
    if (std::strcmp(argv[1], "argv") == 0) {
        passed = Argv();
    }
    if (std::strcmp(argv[1], "argv-boundary") == 0) {
        passed = ArgvBoundary();
    }
    if (std::strcmp(argv[1], "argv-overflow") == 0) {
        passed = ArgvOverflow();
    }
#endif
    return passed ? 0 : 1;
}
