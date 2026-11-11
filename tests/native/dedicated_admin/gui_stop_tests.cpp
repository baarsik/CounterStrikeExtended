#include "admin_actual.inc"
#include "gui_stop_actual.inc"
#include <cstdio>
struct Provider : IGameServerData {
    int writes = 0;
    void WriteDataRequest(const void*, int) override {
        ++writes;
    }
    int ReadDataResponse(void*, int) override {
        return 0;
    }
};
int main() {
    Provider provider;
    RemoteServer().Bind(&provider, 2);
    cse::dedicated::NativeGui retired;
    retired.Stop(); // a closed previous owner's destructor must not revoke a later session
    RemoteServer().SendCommand("status");
    if (provider.writes != 1) {
        std::fputs("FAIL: retired GUI revoked the new binding\n", stderr);
        return 1;
    }
    cse::dedicated::NativeGui active;
    active.bound_ = true;
    active.Stop();
    active.Stop();
    RemoteServer().SendCommand("quit");
    return provider.writes == 1 ? 0 : 1;
}
