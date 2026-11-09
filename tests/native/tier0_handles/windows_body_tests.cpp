#include "fixture_common.h"

// Owned field context for the actual Windows diagnostic bodies on a non-Windows compiler.
// No Windows class layout, TLS, SDK implementation or whole-TU qualification is claimed here.
namespace windows_body {

class CThreadSyncObject {
public:
    CThreadSyncObject();
    void* m_hSyncObject;
    bool m_bOwnEvent;
};

class CThreadEvent : public CThreadSyncObject {
public:
    CThreadEvent(void* event, bool ownEvent);
};

class CThread {
public:
    const char* GetName();
    CThreadMutex m_Lock;
    void* m_hThread = nullptr;
    char m_szName[sizeof(::CThread::m_szName)]{};
};

#define _snprintf ::fixture::Format
#include "actual_windows_diagnostics.inc"
#undef _snprintf

} // namespace windows_body

int main(int argc, char** argv) {
    try {
        fixture::Require(argc == 2, "Expected one scenario");
        constexpr std::uintptr_t HighHandle = UINT64_C(0xfedcba9876543210);
        void* handle = reinterpret_cast<void*>(HighHandle);
        if (std::strcmp(argv[1], "name") == 0) {
            windows_body::CThread thread;
            thread.m_hThread = handle;
            const std::string expected = fixture::ExpectedThreadName(&thread, handle);
            fixture::Require(thread.GetName() == expected, "Windows body lost a diagnostic identity");
            std::printf("Windows method body diagnostic=%s\n", thread.GetName());
        } else if (std::strcmp(argv[1], "event") == 0) {
            windows_body::CThreadEvent event(handle, false);
            fixture::Require(event.m_hSyncObject == handle && !event.m_bOwnEvent,
                "Adopted event handle/ownership changed");
            fixture::CheckEventDiagnostic(handle);
        } else {
            throw std::runtime_error("Unknown scenario");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
