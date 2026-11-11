#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace cse::browser::diagnostics {
// UI-thread callback ordering, enabled only with an absolute output path.
// Store bounded records without I/O; flush after the CEF message loop stops.
class FrameTrace {
public:
    FrameTrace() {
        const char* path = std::getenv("CSE_CEF_FRAME_TRACE");
        if (path && path[0] == '/') {
            path_ = path;
        }
    }

    void Add(std::uint32_t browser, const char* event, int first = 0, int second = 0, int third = 0) {
        if (path_.empty() || count_ == records_.size()) {
            return;
        }
        const double milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
        records_[count_++] = {milliseconds, browser, event, first, second, third};
    }

    void Flush() {
        if (path_.empty() || count_ == 0) {
            return;
        }
        FILE* file = std::fopen(path_.c_str(), "wb");
        if (!file) {
            return;
        }
        std::fputs("milliseconds,browser,event,first,second,third\n", file);
        for (std::size_t index = 0; index < count_; ++index) {
            const auto& record = records_[index];
            std::fprintf(file, "%.6f,%u,%s,%d,%d,%d\n", record.milliseconds, record.browser, record.event,
                record.first, record.second, record.third);
        }
        std::fclose(file);
        count_ = 0;
    }

private:
    using Clock = std::chrono::steady_clock;
    struct Record {
        double milliseconds;
        std::uint32_t browser;
        const char* event;
        int first;
        int second;
        int third;
    };
    constexpr static std::size_t MaximumRecords = 2048;
    std::string path_;
    std::array<Record, MaximumRecords> records_{};
    std::size_t count_ = 0;
    Clock::time_point start_ = Clock::now();
};

inline FrameTrace& Frames() {
    static FrameTrace trace;
    return trace;
}
}
