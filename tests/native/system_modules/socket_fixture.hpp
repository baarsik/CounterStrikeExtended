#pragma once
#include "fixture.hpp"
#include "HLTV/common/INetSocket.h"
#include "HLTV/common/INetChannel.h"
#include "HLTV/common/NetAddress.h"

inline unsigned int socketCloses = 0;
inline unsigned int socketDestructions = 0;
inline unsigned int channelsAtSocketClose = 0;
inline unsigned int socketChannelRemovals = 0;
class ControlledNetwork;

// OS socket operations are controlled. Allocation, list ownership, shutdown and
// deletion use complete extracted production Network methods below.
class ControlledSocket : public INetSocket {
public:
    int m_Port = 0;
    INetwork *network = nullptr;
    ObjectList channels;
    bool Create(ControlledNetwork *owner, int port, bool, bool);
    ~ControlledSocket() override {
        ++socketDestructions;
    }
    void Close() override {
        ++socketCloses;
        channelsAtSocketClose += channels.CountElements();
        // Socket I/O is controlled; every registered channel is the real NetChannel.
        while (auto *channel = static_cast<INetChannel *>(channels.RemoveHead())) {
            channel->Close();
        }
    }
    NetPacket *ReceivePacket() override {
        return nullptr;
    }
    void FreePacket(NetPacket *) override {}
    bool SendPacket(NetPacket *) override {
        return true;
    }
    bool SendPacket(NetAddress *, const void *, int) override {
        return true;
    }
    void AddPacket(NetPacket *) override {}
    bool AddChannel(INetChannel *channel) override {
        return channels.AddTail(channel);
    }
    bool RemoveChannel(INetChannel *channel) override {
        ++socketChannelRemovals;
        return channels.Remove(channel);
    }
    INetwork *GetNetwork() override {
        return network;
    }
    void OutOfBandPrintf(NetAddress *, const char *, ...) override {}
    void Flush() override {}
    void GetFlowStats(float *, float *) override {}
    bool LeaveGroup(NetAddress *) override {
        return true;
    }
    bool JoinGroup(NetAddress *) override {
        return true;
    }
    int GetPort() override {
        return m_Port;
    }
};

class ControlledNetwork : public BaseSystemModule, public INetwork {
public:
    ObjectList m_Sockets;
    NetAddress localAddress;
    void *QueryInterface(const char *name) override {
        if (cse::modules::Is(name, cse::modules::NetworkName)) {
            return static_cast<INetwork *>(this);
        }
        return BaseSystemModule::QueryInterface(name);
    }
    char *GetType() override {
        return const_cast<char *>(cse::modules::NetworkName);
    }
    INetSocket *CreateSocket(int port, bool reuse, bool loopback) override;
    bool RemoveSocket(INetSocket *socket) override;
    ControlledSocket *GetSocket(int port);
    void ShutDown() override;
    NetAddress *GetLocalAddress() override {
        return &localAddress;
    }
    bool ResolveAddress(char *, NetAddress *) override {
        return false;
    }
    void GetFlowStats(float *, float *) override {}
    int GetLastErrorCode() override {
        return 0;
    }
    char *GetErrorText(int) override {
        return const_cast<char *>("controlled");
    }
};

inline bool ControlledSocket::Create(ControlledNetwork *owner, int port, bool, bool) {
    network = owner;
    m_Port = port;
    return true;
}
