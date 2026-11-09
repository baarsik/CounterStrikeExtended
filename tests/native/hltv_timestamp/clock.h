#pragma once

#include <cstdint>

class Clock {
public:
    virtual double GetTime() = 0;
    virtual ~Clock() = default;
};

class WriterExpressions {
public:
    explicit WriterExpressions(Clock *clock) : m_System(clock), startTime(0) {
    }

    void Start();
    float EncodeTime();
    float StartTime() const {
        return startTime;
    }

private:
    Clock *m_System;
    float startTime;
};
