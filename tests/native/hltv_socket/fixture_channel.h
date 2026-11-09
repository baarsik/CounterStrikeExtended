#pragma once
#include "HLTV/common/INetChannel.h"
#include "HLTV/common/NetAddress.h"
#include <vector>

class ControlledChannel : public INetChannel {
public:
    bool Create(IBaseSystem*, INetSocket*, NetAddress*) override {
        return true;
    }
    NetAddress* GetTargetAddress() override {
        return &target;
    }
    void Close() override {
        ++closes;
    }
    void Clear() override {}
    void Reset() override {}
    bool IsConnected() override {
        ++connectedCalls;
        return connected;
    }
    bool IsReadyToSend() override {
        ++readyCalls;
        return ready;
    }
    bool IsCrashed() override {
        return false;
    }
    bool IsTimedOut() override {
        return false;
    }
    bool IsFakeChannel() override {
        return false;
    }
    bool KeepAlive() override {
        ++keepCalls;
        return keep;
    }
    void SetRate(int) override {}
    void SetUpdateRate(int) override {}
    void SetTimeOut(float) override {}
    void SetKeepAlive(bool) override {}
    float GetIdleTime() override {
        return 0;
    }
    int GetRate() override {
        return 0;
    }
    int GetUpdateRate() override {
        return 0;
    }
    float GetLoss() override {
        return 0;
    }
    void TransmitOutgoing() override {
        ++transmits;
    }
    void ProcessIncoming(unsigned char* data, int size) override {
        incoming.assign(data, data + size);
    }
    void OutOfBandPrintf(const char*, ...) override {}
    void FakeAcknowledgement() override {}

    NetAddress target;
    bool ready = true;
    bool keep = true;
    bool connected = true;
    int readyCalls = 0;
    int keepCalls = 0;
    int connectedCalls = 0;
    int transmits = 0;
    int closes = 0;
    std::vector<unsigned char> incoming;
};
