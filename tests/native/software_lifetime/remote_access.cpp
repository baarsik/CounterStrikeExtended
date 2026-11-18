#include "engine/native_software_lifetime.h"
#include "public/game_server_data.h"
#include "engine/utlbuffer.h"
#include "engine/utllinkedlist.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <mach-o/dyld.h>
#define SERVERDATA_REQUESTVALUE 0
#define SERVERDATA_SETVALUE 1
#define SERVERDATA_EXECCOMMAND 2
#define SERVERDATA_UPDATE 1
using namespace cse::software_lifetime;
static const Api* terminal = nullptr;
static bool resourcesReady = false;
static bool hostClosed = false;
static bool reenter = false;
static unsigned int effects = 0;
#include "remote_owner.inc"
static CServerRemoteAccess remote;
void CServerRemoteAccess::SetValue(const char*, const char*) {
    ++effects;
}
void CServerRemoteAccess::ExecCommand(const char*) {
    ++effects;
    if (reenter) {
        assert(terminal->retire() == Result::Busy);
        assert(m_ResponsePackets.Count() != 0);
    }
}
void CServerRemoteAccess::RequestValue(int, const char*) {
    ++effects;
}
namespace cse::software_lifetime {
bool RetireResources(Mode) {
    if (!resourcesReady) {
        return false;
    }
#if !defined(CSE_REMOTE_BASELINE)
    return remote.RetireResponses();
#else
    return true;
#endif
}
} // namespace cse::software_lifetime
static bool Exclusive(void*, const void* base, std::uint64_t epoch) noexcept {
    return hostClosed && base == _dyld_get_image_header(0) && epoch == 1;
}
static void NetworkLog(ESteamNetworkingSocketsDebugOutputType, const char*) {}
extern "C" const Api CseNativeSoftwareLifetimeV1;
int main() {
    terminal = &CseNativeSoftwareLifetimeV1;
    Binding binding{sizeof(Binding), Version,         1, _dyld_get_image_header(0), &remote, Exclusive,
                    NetworkLog,      Mode::Dedicated, 0};
    assert(terminal->bind(&binding) == Result::Ready);
    CUtlBuffer command(0, 0, false);
    command.PutInt(1);
    command.PutInt(SERVERDATA_EXECCOMMAND);
    command.PutString("status");
    command.PutChar(0); // SendCommand writes an empty value after the command.
    remote.SendMessageToAdminUI("queued");
    reenter = true;
    remote.WriteDataRequest(command.Base(), command.TellPut());
    assert(effects == 1);
    reenter = false;
    hostClosed = true;
    assert(terminal->retire() == Result::ResourcesLive);
    assert(remote.m_ResponsePackets.Count() != 0);
    const int received = remote.m_iBytesReceived;
    remote.WriteDataRequest(command.Base(), command.TellPut());
    remote.SendMessageToAdminUI("late notification");
    char response[128]{};
    assert(remote.ReadDataResponse(response, sizeof(response)) == 0);
    assert(effects == 1 && remote.m_iBytesReceived == received && remote.m_ResponsePackets.Count() == 1);
    resourcesReady = true;
    assert(terminal->retire() == Result::Retired && remote.m_ResponsePackets.Count() == 0);
    remote.SendMessageToAdminUI("after retired");
    assert(remote.m_ResponsePackets.Count() == 0 && terminal->retire() == Result::Retired);
    std::puts("PASS actual admin whole-call busy fence, failed-close queue retention, late entry rejection and retry "
              "cleanup");
}
