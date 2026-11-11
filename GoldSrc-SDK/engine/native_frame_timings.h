#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

namespace cse_diagnostics {
// Opt-in measurements allocate once and write only during explicit shutdown.
// Ordinary clients do not collect samples or create a diagnostics file.
class FrameTimings {
public:
    enum class ClientPhase {
        Move,
        ClientDll,
        Packets,
        Prediction,
        Voice,
        Entities,
        Resources,
        Steam,
        Camera,
        Count
    };

    FrameTimings() {
        const char* path = std::getenv("CSE_FRAME_TIME_LOG");
        if (path && path[0] == '/') {
            path_ = path;
        }
    }

    bool Enabled() const {
        return !path_.empty();
    }

    void Sample(double seconds, bool connected, bool menu, bool focused) {
        constexpr std::size_t MaximumSamples = 120000;
        constexpr double MillisecondsPerSecond = 1000.0;
        currentSample_ = false;
        if (samples_.empty() && previous_ == 0.0) {
            samples_.reserve(MaximumSamples);
        }
        if (previous_ != 0.0 && seconds >= previous_ && samples_.size() < MaximumSamples) {
            samples_.push_back({(seconds - previous_) * MillisecondsPerSecond, connected, menu, focused,
                0.0, 0.0, 0.0, 0.0, 0.0, false, {}});
            currentSample_ = true;
        }
        previous_ = seconds;
    }

    void BeginClient(double seconds) {
        clientMark_ = seconds;
    }

    void MarkClient(ClientPhase phase, double seconds) {
        if (!currentSample_ || samples_.empty()) {
            return;
        }
        constexpr double MillisecondsPerSecond = 1000.0;
        samples_.back().clientPhases[static_cast<std::size_t>(phase)] =
            (seconds - clientMark_) * MillisecondsPerSecond;
        clientMark_ = seconds;
    }

    void RecordPacketEvent(int frame, int command, const char* name, double start, double end) {
        constexpr double SlowEventSeconds = 0.001;
        constexpr std::size_t MaximumPacketEvents = 4096;
        if (end - start >= SlowEventSeconds && packetEvents_.size() < MaximumPacketEvents) {
            packetEvents_.push_back({frame, command, name, end - start});
        }
    }

    void Finish(double hostEnd, double serverEnd, double clientEnd, double graphicsEnd, double audioEnd) {
        if (!currentSample_ || samples_.empty()) {
            return;
        }
        constexpr double MillisecondsPerSecond = 1000.0;
        auto& sample = samples_.back();
        sample.host = (hostEnd - previous_) * MillisecondsPerSecond;
        sample.server = (serverEnd - hostEnd) * MillisecondsPerSecond;
        sample.client = (clientEnd - serverEnd) * MillisecondsPerSecond;
        sample.graphics = (graphicsEnd - clientEnd) * MillisecondsPerSecond;
        sample.audio = (audioEnd - graphicsEnd) * MillisecondsPerSecond;
        sample.completed = true;
        currentSample_ = false;
    }

    void Flush() {
        if (path_.empty() || samples_.empty()) {
            return;
        }
        FILE* file = std::fopen(path_.c_str(), "ab");
        if (!file) {
            return;
        }
        std::fprintf(file, "# pid=%d\ninterval_ms,connected,menu,focused,host_ms,server_ms,client_ms,"
            "graphics_ms,audio_ms,completed,move_ms,client_dll_ms,packets_ms,prediction_ms,voice_ms,"
            "entities_ms,resources_ms,steam_ms,camera_ms\n", getpid());
        for (const auto& sample : samples_) {
            std::fprintf(file, "%.6f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d", sample.milliseconds,
                sample.connected, sample.menu, sample.focused, sample.host, sample.server, sample.client,
                sample.graphics, sample.audio, sample.completed);
            for (double phase : sample.clientPhases) {
                std::fprintf(file, ",%.6f", phase);
            }
            std::fputc('\n', file);
        }
        std::fclose(file);
        if (!packetEvents_.empty()) {
            std::string eventPath = path_ + ".messages.csv";
            FILE* events = std::fopen(eventPath.c_str(), "ab");
            if (events) {
                constexpr double MillisecondsPerSecond = 1000.0;
                std::fprintf(events, "frame,command,name,milliseconds\n");
                for (const auto& event : packetEvents_) {
                    std::fprintf(events, "%d,%d,%s,%.6f\n", event.frame, event.command, event.name.c_str(),
                        event.seconds * MillisecondsPerSecond);
                }
                std::fclose(events);
            }
        }
        packetEvents_.clear();
        samples_.clear();
        previous_ = 0.0;
        currentSample_ = false;
    }

private:
    struct PacketEvent {
        int frame;
        int command;
        std::string name;
        double seconds;
    };

    struct SampleValue {
        double milliseconds;
        bool connected;
        bool menu;
        bool focused;
        double host;
        double server;
        double client;
        double graphics;
        double audio;
        bool completed;
        std::array<double, static_cast<std::size_t>(ClientPhase::Count)> clientPhases;
    };
    std::string path_;
    std::vector<SampleValue> samples_;
    std::vector<PacketEvent> packetEvents_;
    double previous_ = 0.0;
    double clientMark_ = 0.0;
    bool currentSample_ = false;
};

inline FrameTimings& NativeFrameTimings() {
    static FrameTimings timings;
    return timings;
}
}
