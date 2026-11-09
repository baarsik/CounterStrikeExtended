#include "VGUI.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "VGUI formatting failure: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

int FileDescriptor(FILE* file) {
#if defined(_WIN32)
    return _fileno(file);
#else
    return fileno(file);
#endif
}

int Duplicate(int descriptor) {
#if defined(_WIN32)
    return _dup(descriptor);
#else
    return dup(descriptor);
#endif
}

bool Redirect(int source, int destination) {
#if defined(_WIN32)
    return _dup2(source, destination) == 0;
#else
    return dup2(source, destination) != -1;
#endif
}

void Close(int descriptor) {
#if defined(_WIN32)
    _close(descriptor);
#else
    close(descriptor);
#endif
}

class CaptureOutput {
public:
    CaptureOutput() : file_(std::tmpfile()) {
        Require(file_ != nullptr, "create owned output capture");
        std::fflush(stdout);
        original_ = Duplicate(FileDescriptor(stdout));
        Require(original_ != -1, "save stdout descriptor");
        Require(Redirect(FileDescriptor(file_), FileDescriptor(stdout)), "capture actual production stdout");
    }

    ~CaptureOutput() {
        if (original_ != -1) {
            Restore();
        }
        std::fclose(file_);
    }

    std::string Finish() {
        Restore();
        std::rewind(file_);
        std::string bytes;
        char chunk[256] = {};
        std::size_t count = 0;
        while ((count = std::fread(chunk, 1, sizeof(chunk), file_)) != 0) {
            bytes.append(chunk, count);
        }
        Require(!std::ferror(file_), "read captured production output");
        return bytes;
    }

    CaptureOutput(const CaptureOutput&) = delete;
    CaptureOutput& operator=(const CaptureOutput&) = delete;

private:
    void Restore() {
        std::fflush(stdout);
        Require(Redirect(original_, FileDescriptor(stdout)), "restore stdout descriptor");
        Close(original_);
        original_ = -1;
    }

    FILE* file_;
    int original_ = -1;
};

template <typename... Arguments>
void Expect(const std::string& expectedOutput, int expectedReturn, const char* format, Arguments... arguments) {
    CaptureOutput capture;
    const int result = vgui::vgui_printf(format, arguments...);
    const std::string output = capture.Finish();
    Require(result == expectedReturn, "production return count matches formatted byte count");
    Require(output == expectedOutput, "production stdout bytes match the expected result");
}

void TestDefinedOutput() {
    Expect("", 0, "%s", "");
    const std::string formatted = "score -17 0x2a 000023 3.50 % !";
    Expect(formatted, static_cast<int>(formatted.size()), "%s %d %#x %06u %.2f %% %c",
        "score", -17, 42u, 23u, 3.5, '!');
    const char* unicodeBytes = u8"Zażółć Ελληνικά 日本語";
    Expect(unicodeBytes, static_cast<int>(std::strlen(unicodeBytes)), "%s", unicodeBytes);
    Expect("A", 3, "A%cB", 0);
    constexpr int maximumDefinedBytes = 2047;
    const std::string exactFit(maximumDefinedBytes, 'x');
    Expect(exactFit, maximumDefinedBytes, "%s", exactFit.c_str());
}

void TestOverlongOutput() {
    constexpr int requestBytes = 4096;
    constexpr int maximumPrintedBytes = 2047;
    const std::string overlong(requestBytes, 'y');
    Expect(std::string(maximumPrintedBytes, 'y'), requestBytes, "%s", overlong.c_str());
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "select defined or overlong formatting case");
    if (std::strcmp(argv[1], "defined") == 0) {
        TestDefinedOutput();
    } else {
        Require(std::strcmp(argv[1], "overlong") == 0, "known formatting case");
        TestOverlongOutput();
    }
    std::printf("Native VGUI formatting %s passed\n", argv[1]);
    return EXIT_SUCCESS;
}
