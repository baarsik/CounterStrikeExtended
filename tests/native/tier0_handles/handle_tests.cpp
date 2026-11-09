#include "fixture_common.h"
#include "tier0/vprof.h"

#include <climits>
#include <cstdlib>
#include <new>
#include <thread>

#include "../strings/high_offset_buffer.h"

extern "C" void Error(const char*, ...) {
    std::fputs("Production TLS allocation failed\n", stderr);
    std::abort();
}

#if defined(_WIN32)
#include "actual_windows_tls.inc"
#else
#include "actual_posix_tls.inc"
#endif

#define _snprintf ::fixture::Format
#define snprintf ::fixture::Format
#if defined(_WIN32)
#include "actual_windows_thread.inc"
#else
#include "actual_posix_thread.inc"
#endif
#undef snprintf
#undef _snprintf

// Only the diagnostics run: no worker is created, joined or stopped by these lifecycle bindings.
CThread::~CThread() {
}

bool CThread::Start(unsigned int) {
    return false;
}

bool CThread::Init() {
    return true;
}

bool CThread::WaitForCreateComplete(CThreadEvent*) {
    return false;
}

CThread::ThreadProc_t CThread::GetThreadProc() {
    return nullptr;
}

#if defined(_WIN32)
CThreadSyncObject::~CThreadSyncObject() {
}
#endif

// Node timing/tree services are controlled bindings; the complete profile gate methods below are production code.
CL2Cache::CL2Cache() {
    m_nID = 0;
    m_pL2CacheEvent = nullptr;
    m_i64Start = 0;
    m_i64End = 0;
    m_iL2CacheMissCount = 0;
}

CL2Cache::~CL2Cache() {
}

CVProfNode::CVProfNode(const char* name, int, CVProfNode* parent, const char*, int) {
    m_pszName = new char('\0');
    m_pvOrigNameAddress = name;
    m_iCurL2CacheMiss = 0;
    m_iTotalL2CacheMiss = 0;
    m_nRecursions = 0;
    m_nCurFrameCalls = 0;
    m_nPrevFrameCalls = 0;
    m_nTotalCalls = 0;
    m_pParent = parent;
    m_pChild = nullptr;
    m_pSibling = nullptr;
    m_BudgetGroupID = 0;
    m_iClientData = 0;
    m_iUniqueNodeID = 0;
}

void CVProfNode::EnterScope() {
    ++m_nRecursions;
    ++m_nCurFrameCalls;
}

bool CVProfNode::ExitScope() {
    fixture::Require(m_nRecursions > 0, "Unbalanced fixture node scope");
    --m_nRecursions;
    return m_nRecursions == 0 && m_pParent != nullptr;
}

CVProfNode* CVProfNode::GetSubNode(const char* name, int detail, const char* budget, int flags) {
    if (!m_pChild) {
        m_pChild = new CVProfNode(name, detail, this, budget, flags);
    }
    return m_pChild;
}

CVProfile::CVProfile() : m_Root("Root", 0, nullptr, "Unaccounted", 0) {
    m_enabled = 0;
    m_fAtRoot = true;
    m_pCurNode = &m_Root;
    m_nFrames = 0;
    m_ProfileDetailLevel = 0;
    m_pausedEnabledDepth = 0;
    m_bThreadSafe = false;
    m_iMainThreadSerial = 1;
    m_pBudgetGroups = new CBudgetGroup[1]{};
    m_nBudgetGroupNames = 1;
    m_nBudgetGroupNamesAllocated = 1;
    m_pNumBudgetGroupsChangedCallBack = nullptr;
    m_bPMEInit = false;
    m_bPMEEnabled = false;
}

CVProfile::~CVProfile() {
    delete[] m_pBudgetGroups;
#if defined(_WIN32)
    delete m_Root.m_pChild;
#endif
}

#include "actual_profile.inc"

namespace {

class DiagnosticThread : public CThread {
public:
    int Run() override {
        return 0;
    }
};

constexpr std::size_t GuardBytes = 16;
constexpr std::size_t ObjectOffset = 4096;
constexpr unsigned char GuardByte = 0xa5;
#if UINTPTR_MAX > UINT32_MAX
constexpr std::uintptr_t DiagnosticThreadBits = UINT64_C(0xfedcba9876543210);
#else
constexpr std::uintptr_t DiagnosticThreadBits = UINT32_C(0xabcdef90);
#endif

int SerialWithBits(std::uint32_t bits) {
    static_assert(sizeof(int) == sizeof(bits), "Actual profiler serial storage must remain 32-bit");
    int serial = 0;
    std::memcpy(&serial, &bits, sizeof(serial));
    return serial;
}

void* SerialToken(std::uintptr_t bits) {
    return reinterpret_cast<void*>(bits);
}

void SetDiagnosticThread(CThread& thread) {
#if defined(_WIN32)
    thread.m_hThread = reinterpret_cast<void*>(DiagnosticThreadBits);
#else
    thread.m_threadId = fixture::OpaqueThreadId<pthread_t>(DiagnosticThreadBits);
#endif
}

void Layout() {
    DiagnosticThread thread;
    constexpr std::size_t NativeNameBytes = 64;
    constexpr std::size_t Win32NameBytes = 32;
    constexpr std::size_t ExpectedNameBytes = sizeof(void*) > sizeof(std::uint32_t)
        ? NativeNameBytes : Win32NameBytes;
    const auto object = reinterpret_cast<std::uintptr_t>(&thread);
    const auto name = reinterpret_cast<std::uintptr_t>(&thread.m_szName);
    std::printf("CThread size=%zu alignment=%zu name-offset=%zu name-capacity=%zu TLS-index=%zu serial=%zu\n",
        sizeof(CThread), alignof(CThread), static_cast<std::size_t>(name - object), sizeof(thread.m_szName),
        sizeof(CThreadLocalBase::m_index), sizeof(CVProfile::m_iMainThreadSerial));
#if !defined(_WIN32)
    std::printf("actual pthread_t bytes=%zu pointer-type=%d\n", sizeof(pthread_t),
        static_cast<int>(std::is_pointer<pthread_t>::value));
#endif
    fixture::Require(sizeof(thread.m_szName) == ExpectedNameBytes, "Unexpected diagnostic-name capacity");
    static_assert(sizeof(CVProfile::m_iMainThreadSerial) == sizeof(std::int32_t), "Profiler counter widened");
}

void ThreadName() {
    HighOffsetBuffer region(nullptr);
    fixture::Require(region.get() != nullptr, "Cannot allocate diagnostic object region");
    char* storage = region.get() + ObjectOffset;
    std::memset(storage - GuardBytes, GuardByte, GuardBytes);
    std::memset(storage + sizeof(DiagnosticThread), GuardByte, GuardBytes);
    auto* thread = new (storage) DiagnosticThread;
    SetDiagnosticThread(*thread);
#if UINTPTR_MAX > UINT32_MAX
    fixture::Require(reinterpret_cast<std::uintptr_t>(thread) > UINT32_MAX, "Object must have high address bits");
#endif
    const std::string expected = fixture::ExpectedThreadName(thread, reinterpret_cast<void*>(DiagnosticThreadBits));
    const char* result = thread->GetName();
    std::printf("thread diagnostic=%s expected=%s\n", result, expected.c_str());
    fixture::Require(result == thread->m_szName, "Thread name return storage/lifetime changed");
    fixture::Require(result == expected, "Thread name lost object/thread identity or changed intended Win32 text");
    fixture::Require(thread->GetName() == result, "Thread name pointer is unstable");
    for (std::size_t index = 0; index < GuardBytes; ++index) {
        fixture::Require(static_cast<unsigned char>((storage - GuardBytes)[index]) == GuardByte,
            "Name formatting overwrote the leading object guard");
        fixture::Require(static_cast<unsigned char>(storage[sizeof(DiagnosticThread) + index]) == GuardByte,
            "Name formatting overwrote the trailing object guard");
    }
    thread->~DiagnosticThread();
}

void CachedName() {
    DiagnosticThread thread;
    SetDiagnosticThread(thread);
    thread.SetName("worker-custom");
    fixture::Require(std::strcmp(thread.GetName(), "worker-custom") == 0, "Named-thread cache behavior changed");
    const std::string longName(128, 'n');
    thread.SetName(longName.c_str());
    fixture::Require(std::strlen(thread.GetName()) == sizeof(thread.m_szName) - 1, "Name is not bounded/terminated");
    fixture::Require(thread.GetName()[sizeof(thread.m_szName) - 1] == '\0', "Name terminator was overwritten");
}

void TlsBits() {
    CThreadLocal<int> slot;
    fixture::Require(slot.Get() == nullptr, "New native TLS slot must start empty");
    constexpr std::uintptr_t Values[] = {
        0, 1, UINT32_C(0x7fffffff), UINT32_C(0x80000000), UINT32_C(0xffffffff),
#if UINTPTR_MAX > UINT32_MAX
        UINT64_C(0x00000012abcdef90), UINT64_C(0xffffffff80000000)
#endif
    };
    for (const auto value : Values) {
        slot.Set(SerialToken(value));
        fixture::Require(reinterpret_cast<std::uintptr_t>(slot.Get()) == value, "Native TLS API changed token bits");
    }
    slot.Set(nullptr);
}

void SerialStart() {
    struct SerialCase {
        std::uint32_t before;
        std::uint32_t after;
    };
    constexpr SerialCase Cases[] = {
        {1, 2}, {0x7ffffffe, 0x7fffffff}, {0x80000000, 0x80000001}, {0xfffffffe, 0xffffffff}, {0xffffffff, 0}
    };
    for (const auto& expected : Cases) {
        CVProfile profile;
        profile.m_iMainThreadSerial = SerialWithBits(expected.before);
        profile.Start();
        const auto token = reinterpret_cast<std::uintptr_t>(profile.m_iThreadLocalSerial.Get());
        std::printf("serial before=%08x expected=%08x token=%p\n", static_cast<unsigned int>(expected.before),
            static_cast<unsigned int>(expected.after), profile.m_iThreadLocalSerial.Get());
        fixture::Require(token == expected.after, "Profiler serial token is not the explicit zero-extended 32-bit identity");
        fixture::Require(static_cast<std::uint32_t>(profile.m_iMainThreadSerial) == expected.after,
            "Profiler counter bits changed");
        const int serial = profile.m_iMainThreadSerial;
        profile.Start();
        fixture::Require(profile.m_iMainThreadSerial == serial && profile.m_Root.m_nCurFrameCalls == 1,
            "Nested Start changed serial/root entry semantics");
        profile.Stop();
        fixture::Require(profile.m_enabled == 1, "Nested Stop depth changed");
        profile.Stop();
        fixture::Require(profile.m_enabled == 0 && profile.m_Root.m_nRecursions == 0, "Start/Stop unbalanced");
    }
}

void ScopeGates() {
    constexpr std::uint32_t SerialBits = UINT32_C(0x80000001);
    CVProfile profile;
    profile.m_iMainThreadSerial = SerialWithBits(SerialBits - 1);
    profile.m_bThreadSafe = true;
    profile.Start();
    profile.EnterScope("child", 0, "budget", false, BUDGETFLAG_SERVER);
    fixture::Require(!profile.m_fAtRoot && profile.m_pCurNode != &profile.m_Root, "Eligible thread did not enter");
    fixture::Require(profile.m_pBudgetGroups[0].m_BudgetFlags == BUDGETFLAG_SERVER, "Budget flags changed");
    CVProfNode* child = profile.m_pCurNode;
    profile.m_iThreadLocalSerial.Set(SerialToken(SerialBits + 1));
    profile.ExitScope();
    fixture::Require(profile.m_pCurNode == child && child->m_nRecursions == 1, "Foreign serial exited another scope");
    profile.m_iThreadLocalSerial.Set(SerialToken(SerialBits));
    profile.ExitScope();
    fixture::Require(profile.m_fAtRoot && profile.m_pCurNode == &profile.m_Root, "Eligible exit did not return to root");
    const unsigned int entries = child->m_nCurFrameCalls;
    profile.m_iThreadLocalSerial.Set(SerialToken(SerialBits + 1));
    profile.EnterScope("child", 0, "budget", false);
    fixture::Require(child->m_nCurFrameCalls == entries && profile.m_fAtRoot, "Foreign serial entered a scope");
#if UINTPTR_MAX > UINT32_MAX
    constexpr std::uintptr_t HighBits = UINT64_C(0x0000001200000000);
    profile.m_iThreadLocalSerial.Set(SerialToken(HighBits | SerialBits));
    profile.EnterScope("child", 0, "budget", false);
    fixture::Require(!profile.m_fAtRoot, "Existing signed-low32 token eligibility changed");
    profile.ExitScope();
#endif
    profile.m_bThreadSafe = false;
    profile.m_iThreadLocalSerial.Set(nullptr);
    profile.EnterScope("child", 0, "budget", false);
    fixture::Require(!profile.m_fAtRoot, "Non-thread-safe mode no longer bypasses the serial gate");
    profile.ExitScope();
    profile.Stop();
    const unsigned int stoppedEntries = child->m_nCurFrameCalls;
    profile.EnterScope("child", 0, "budget", false);
    fixture::Require(child->m_nCurFrameCalls == stoppedEntries && profile.m_fAtRoot,
        "Stopped root profile entered a scope");
}

void TlsIsolation() {
    CVProfile profile;
    profile.m_bThreadSafe = true;
    profile.Start();
    void* mainToken = profile.m_iThreadLocalSerial.Get();
    bool workerEmpty = false;
    bool workerBlocked = false;
    std::thread worker([&]() {
        workerEmpty = profile.m_iThreadLocalSerial.Get() == nullptr;
        profile.EnterScope("worker", 0, "budget", false);
        workerBlocked = profile.m_fAtRoot && profile.m_Root.GetChild() == nullptr;
        profile.m_iThreadLocalSerial.Set(SerialToken(UINT32_C(0xfedcba98)));
    });
    worker.join();
    fixture::Require(workerEmpty && workerBlocked, "A different native TLS thread was eligible");
    fixture::Require(profile.m_iThreadLocalSerial.Get() == mainToken, "Worker token replaced main TLS value");
    profile.Stop();
}

#if defined(_WIN32)
void EventHandle() {
    void* handle = reinterpret_cast<void*>(DiagnosticThreadBits);
    CThreadEvent event(handle, false);
    fixture::Require(event.Handle() == handle && !event.m_bOwnEvent, "Event adoption/ownership changed");
    fixture::CheckEventDiagnostic(handle);
}
#endif

} // namespace

int main(int argc, char** argv) {
    try {
        fixture::Require(argc == 2, "Expected one scenario");
        const std::string scenario = argv[1];
        if (scenario == "layout") {
            Layout();
        } else if (scenario == "thread-name") {
            ThreadName();
        } else if (scenario == "cached-name") {
            CachedName();
        } else if (scenario == "tls-bits") {
            TlsBits();
        } else if (scenario == "serial-start") {
            SerialStart();
        } else if (scenario == "scope-gates") {
            ScopeGates();
        } else if (scenario == "tls-isolation") {
            TlsIsolation();
#if defined(_WIN32)
        } else if (scenario == "event") {
            EventHandle();
#endif
        } else {
            throw std::runtime_error("Unknown scenario");
        }
        std::printf("PASS %s\n", scenario.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
