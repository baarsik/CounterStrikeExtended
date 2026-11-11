#include "native_platform.h"
#include "ia3dapi.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)
// Labeled local test doubles: no ABI or real provider acceptance is implied.
struct BufferFixture {
    int references = 3;
    bool failLock = false;
    bool failUnlock = false;
    BYTE bytes[4]{};
    void Release() {
        --references;
    }

    HRESULT Lock(DWORD, DWORD, void** first, DWORD* firstBytes, void** second, DWORD* secondBytes, DWORD) {
        if (failLock) {
            return E_FAIL;
        }
        *first = bytes;
        *firstBytes = 2;
        *second = bytes + 2;
        *secondBytes = 2;
        return S_OK;
    }

    HRESULT Unlock(void*, DWORD, void*, DWORD) {
        return failUnlock ? E_FAIL : S_OK;
    }
};
class CA3dSource {
public:
    DWORD m_nSamplesPerSec = 0;
    DWORD m_nBlockAlign = 0;
    WORD m_wBitsPerSample = 0;
    DWORD m_nChannels = 0;
    WORD m_wFormatTag = 0;
    DWORD m_nWaveSize = 0;
    DWORD m_nAvgBytesPerSec = 0;
    BufferFixture* m_pBuffer = nullptr;
    BufferFixture* m_pBuffer3d = nullptr;
    BufferFixture* m_pSourceBackend = nullptr;
    BufferFixture buffer;
    bool failAllocate = false;
    HRESULT AllocateWaveData(INT) {
        m_pBuffer = &buffer;
        m_pBuffer3d = &buffer;
        m_pSourceBackend = &buffer;
        return failAllocate ? E_FAIL : S_OK;
    }
    HRESULT STDMETHODCALLTYPE LoadWaveFile(LPSTR filename);
};
#include "load_wave_body.inc"
int main(int argc, char** argv) {
    CHECK(argc == 2);
    BYTE wave[] = {'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x22,0x56,0,0,0x44,0xac,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,1,2,3,4};
    char path[] = "/tmp/cse-a3d-caller-XXXXXX";
    const int file = mkstemp(path);
    CHECK(file >= 0 && write(file, wave, sizeof(wave)) == sizeof(wave));
    CHECK(close(file) == 0);
    CA3dSource source;
    source.failAllocate = !std::strcmp(argv[1], "caller_allocate_failure");
    source.buffer.failLock = !std::strcmp(argv[1], "caller_lock_failure");
    source.buffer.failUnlock = !std::strcmp(argv[1], "caller_unlock_failure");
    const HRESULT result = source.LoadWaveFile(path);
    CHECK(unlink(path) == 0);
    if (!std::strcmp(argv[1], "caller_valid")) {
        CHECK(result == S_OK);
        CHECK(!std::memcmp(source.buffer.bytes, wave + 44, 4));
    } else {
        CHECK(FAILED(result));
        CHECK(!source.m_pBuffer && !source.m_pBuffer3d && !source.m_pSourceBackend);
        CHECK(source.buffer.references == 0);
    }
}
