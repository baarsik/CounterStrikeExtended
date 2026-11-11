#include "socket_fixture.hpp"
#include "HLTV/Proxy/FakeClient.h"
#include "HLTV/Proxy/Proxy.h"
#include "HLTV/Proxy/ProxyClient.h"

extern "C" void *Mem_ZeroMalloc(std::size_t size) {
    return std::calloc(1, size);
}
SystemWrapper gSystemWrapper;
SystemWrapper *currentModuleSystem = nullptr;
int COM_BuildNumber() {
    return 1;
}

IFileSystem *ConnectedFixtureFileSystem();

class FakeClientSystem : public SystemWrapper {
public:
    ControlledNetwork network;
    IFileSystem *GetFileSystem() override {
        if (failFileSystem) {
            throw std::bad_alloc();
        }
        return ConnectedFixtureFileSystem();
    }
    ISystemModule *GetModule(char *type, char *library, char *name) override {
        if (cse::modules::Is(type, cse::modules::NetworkName)) {
            if (!m_NativeModules.Find(&network)) {
                Require(AddModule(&network, const_cast<char *>("network")), "network setup failed");
            }
            return &network;
        }
        return SystemWrapper::GetModule(type, library, name);
    }
};

class ShutdownObserver : public BaseSystemModule {
public:
    bool notified = false;
    void ReceiveSignal(ISystemModule *publisher, unsigned int signal, void *) override {
        Require(signal == cse::modules::NetworkShutdownSignal, "unexpected network shutdown signal");
        notified = true;
        Require(!cse::modules::Subscribe(publisher, this), "shutdown allowed a new listener");
        auto *network = cse::modules::Query<INetwork>(publisher, cse::modules::NetworkName);
        Require(network && !network->CreateSocket(0, false, false), "shutdown allowed a new socket");
    }
};

int main(int argc, char **argv) {
    try {
        Require(argc == 3, "core path and success/failure mode required");
        FakeClientSystem system;
        currentModuleSystem = &system;
        system.corePath = argv[1];
        Require(system.Init(&system, 0, const_cast<char *>("fakeclient-test")), "system init failed");
        const std::string mode = argv[2];
        if (mode != "success" && mode != "failure") {
            Proxy proxy;
            Require(system.AddModule(&proxy, const_cast<char *>("proxy")), "proxy setup failed");
            NetAddress remote;
            remote.m_IP[0] = 127;
            remote.m_IP[3] = 1;
            remote.SetPort_(27015);
            const bool fake = mode == "fake-stop" || mode == "fake-parent";
            FakeClient fakeClient;
            if (fake) {
                Require(system.AddModule(&fakeClient, const_cast<char *>("fake")), "fake setup failed");
                fakeClient.Connect(&remote);
            } else {
                auto *client = new ProxyClient(&proxy);
                Require(system.AddModule(client, const_cast<char *>("spectator")), "spectator setup failed");
                proxy.GetClients()->Add(static_cast<IClient *>(client));
                client->SetWorld(proxy.GetWorld());
                Require(client->Connect(proxy.GetSocket(), &remote, const_cast<char *>("\\name\\spectator")),
                    "spectator connection failed");
            }
            Require(socketChannelRemovals == 0, "channel unexpectedly closed before test");
            ShutdownObserver observer;
            Require(cse::modules::Subscribe(&system.network, &observer), "observer subscription failed");
            if (mode == "proxy-remove" || mode == "fake-parent") {
                system.RemoveModule(&proxy);
            } else if (mode == "network-remove") {
                system.RemoveModule(&system.network);
            } else {
                system.ShutDown();
            }
            Require(observer.notified, "network skipped shutdown notification");
            Require(channelsAtSocketClose == 0, "socket released before connected channel owner shut down");
            Require(socketChannelRemovals == 1, "connected channel was not detached exactly once");
            Require(socketCloses == (fake ? 2u : 1u), "provider socket close count wrong");
            Require(socketDestructions == socketCloses, "provider socket destruction count wrong");
            Require(proxy.GetClients()->CountElements() == 0, "spectator survived parent teardown");
            Require(!system.m_NativeModules.Find(&proxy), "Proxy survived network teardown");
            if (fake) {
                Require(!system.m_NativeModules.Find(&fakeClient), "FakeClient survived network teardown");
            }
            system.ShutDown();
            Require(socketCloses == (fake ? 2u : 1u) && socketDestructions == socketCloses,
                "connected socket cleanup repeated");
            return 0;
        }
        Require(system.AddModule(&system.network, const_cast<char *>("network")), "network setup failed");
        const bool failInit = std::strcmp(argv[2], "failure") == 0;
        system.failFileSystem = failInit;
        FakeClient client;
        const bool initialized = system.AddModule(&client, const_cast<char *>("fakeclient"));
        Require(initialized != failInit, "FakeClient Init result wrong");
        if (initialized) {
            system.RemoveModule(&client);
        }
        // FakeClient's borrowed socket must stay provider-owned until Network shutdown.
        Require(system.network.m_Sockets.CountElements() == 1, "FakeClient stranded the provider socket");
        Require(socketCloses == 0 && socketDestructions == 0, "consumer released provider socket");
        system.network.ShutDown();
        Require(socketCloses == 1, "provider did not close the owned socket");
        Require(socketDestructions == 1, "provider did not destroy the owned socket");
        system.network.ShutDown();
        system.ShutDown();
        Require(socketCloses == 1 && socketDestructions == 1, "socket cleanup repeated");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
