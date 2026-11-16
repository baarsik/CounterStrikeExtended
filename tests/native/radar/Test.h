#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <sstream>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <ncl_math/vec3.h>

namespace radar_test {
struct Case {
    const char* name;
    void (*run)();
};

inline std::vector<Case>& Cases() {
    static std::vector<Case> cases;
    return cases;
}

struct Register {
    Register(const char* name, void (*run)()) {
        Cases().push_back({name, run});
    }
};

inline thread_local std::vector<std::string> traces;

class Message {
  private:
    std::ostringstream text_;

  public:
    template <typename Value> Message& operator<<(const Value& value) {
        text_ << value;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& output, const Message& message) {
        return output << message.text_.str();
    }
};

class Trace {
  public:
    template <typename Value> explicit Trace(const Value& value) {
        std::ostringstream output;
        output << value;
        traces.push_back(output.str());
    }

    ~Trace() {
        traces.pop_back();
    }
};

class Check {
  public:
    Check(bool success, const char* expression, const char* file, int line) {
        if (!success) {
            std::string message = std::string(file) + ":" + std::to_string(line) + ": " + expression;
            for (const auto& trace : traces) {
                message += " [" + trace + "]";
            }
            throw std::runtime_error(message);
        }
    }

    template <typename Value> Check& operator<<(const Value&) {
        return *this;
    }
};

inline bool FloatEqual(float left, float right) {
    if (std::isnan(left) || std::isnan(right)) {
        return false;
    }
    constexpr uint32_t sign_bit = uint32_t{1} << 31;
    constexpr uint32_t max_ulps = 4;
    const auto ordered = [](float value) {
        const auto bits = std::bit_cast<uint32_t>(value);
        return (bits & sign_bit) ? ~bits + 1u : bits | sign_bit;
    };
    const auto left_bits = ordered(left);
    const auto right_bits = ordered(right);
    const auto distance = left_bits > right_bits ? left_bits - right_bits : right_bits - left_bits;
    return distance <= max_ulps;
}
} // namespace radar_test

#define RADAR_TEST(suite, name)                                                                                        \
    static void suite##_##name();                                                                                      \
    static radar_test::Register suite##_##name##_registration(#suite "." #name, suite##_##name);                       \
    static void suite##_##name()
#define CHECK_TRUE(expression) radar_test::Check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define CHECK_FALSE(expression) CHECK_TRUE(!(expression))
#define CHECK_EQ(left, right) CHECK_TRUE((left) == (right))
#define CHECK_NE(left, right) CHECK_TRUE((left) != (right))
#define CHECK_LT(left, right) CHECK_TRUE((left) < (right))
#define CHECK_LE(left, right) CHECK_TRUE((left) <= (right))
#define CHECK_GT(left, right) CHECK_TRUE((left) > (right))
#define CHECK_GE(left, right) CHECK_TRUE((left) >= (right))
#define CHECK_NEAR(left, right, tolerance) CHECK_TRUE(std::abs((left) - (right)) <= (tolerance))
#define CHECK_FLOAT_EQ(left, right) CHECK_TRUE(radar_test::FloatEqual((left), (right)))
#define CHECK_STREQ(left, right) CHECK_TRUE(std::strcmp((left), (right)) == 0)
#define RADAR_JOIN_DETAIL(left, right) left##right
#define RADAR_JOIN(left, right) RADAR_JOIN_DETAIL(left, right)
#define RADAR_TRACE(value) radar_test::Trace RADAR_JOIN(radar_trace_, __LINE__)(value)
#define RADAR_TEST_F(fixture, name)                                                                                    \
    class fixture##_##name : public fixture {                                                                          \
      public:                                                                                                          \
        void Run();                                                                                                    \
        static void Execute() {                                                                                        \
            fixture##_##name test;                                                                                     \
            test.SetUp();                                                                                              \
            test.Run();                                                                                                \
        }                                                                                                              \
    };                                                                                                                 \
    static radar_test::Register fixture##_##name##_registration(#fixture "." #name, fixture##_##name::Execute);        \
    void fixture##_##name::Run()
