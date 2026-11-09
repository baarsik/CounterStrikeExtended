#include "webm_video.h"
#define CSE_WEBM_NO_DOMAIN_MAIN
#include "libwebm_domain_probe.cpp"

bool FixtureStoreFrameSize(long nativeSize, int& size);

namespace {

unsigned int allocationCalls = 0;
unsigned int failAllocationCall = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "WebM demux boundary fixture: %s\n", message);
        std::exit(1);
    }
}

void FrameAdmission() {
#if !defined(CSE_WEBM_OLD_SOURCE)
    int size = 17;
    Require(!FixtureStoreFrameSize(-1, size) && size == 0, "negative frame storage is rejected without narrowing");
    Require(!FixtureStoreFrameSize(0, size) && size == 0, "zero frame storage remains inadmissible");
    Require(FixtureStoreFrameSize(1, size) && size == 1, "minimum positive frame size is retained");
    Require(FixtureStoreFrameSize(INT_MAX, size) && size == INT_MAX, "largest Win32 frame size is retained");
#if LONG_MAX > INT_MAX
    Require(!FixtureStoreFrameSize(static_cast<long>(INT_MAX) + 1, size) && size == 0,
        "first LP64-only frame size cannot wrap");
    Require(!FixtureStoreFrameSize(LONG_MAX, size) && size == 0, "maximum LP64 frame size cannot wrap");
#endif
#endif
#if LONG_MAX > INT_MAX
    SparseReader reader = MakeContainer(1, 1, 1, static_cast<std::uint64_t>(INT_MAX) + 1);
    WebMDemuxer demuxer(&reader, 0, 0);
    WebMFrame frame;
    Require(demuxer.m_bIsOpen, "native library admits the expanded LP64 frame domain");
    const unsigned int before = allocationCalls;
    Require(!demuxer.ReadFrame(&frame, nullptr) && frame.m_uBufferSize == 0 && frame.m_uBufferCapacity == 0
            && frame.m_pBuffer == nullptr && allocationCalls == before && reader.payloadReads == 0,
        "frame beyond Win32 LONG_MAX rejected before allocation, buffer mutation, or payload read");
#endif
}

void ReallocationFailureRetry() {
    SparseReader reader = MakeContainer(1, 1, 1, 1, {64, 64});
    WebMDemuxer demuxer(&reader, 0, 0);
    WebMFrame frame;
    allocationCalls = 0;
    failAllocationCall = 2;
    Require(demuxer.m_bIsOpen && demuxer.ReadFrame(&frame, nullptr) && frame.m_uBufferSize == 1
            && frame.m_uBufferCapacity == 1 && allocationCalls == 1,
        "actual SDK demuxer allocates first small packet");
    unsigned char* const firstBuffer = frame.m_pBuffer;
    Require(!demuxer.ReadFrame(&frame, nullptr) && frame.m_uBufferSize == 0
            && frame.m_uBufferCapacity == 1 && frame.m_pBuffer == firstBuffer && allocationCalls == 2,
        "failed growth retains previous actual storage capacity and pointer");
    Require(demuxer.ReadFrame(&frame, nullptr) && frame.m_uBufferSize == 64
            && frame.m_uBufferCapacity == 64 && allocationCalls == 3,
        "later packet retries growth instead of writing through the failed capacity");
    failAllocationCall = 0;
}

void BaselineAlias() {
    SparseReader reader = MakeContainer(1, 1, 0x100000001ull, 1);
    WebMDemuxer demuxer(&reader, 0, 0);
    WebMFrame frame;
    Require(demuxer.m_bIsOpen && demuxer.ReadFrame(&frame, nullptr) && frame.m_uBufferSize == 1,
        "existing low32 Block-track alias behavior is explicitly retained");
}

}

void* FixtureReallocate(void* buffer, std::size_t bytes) {
    ++allocationCalls;
    if (allocationCalls == failAllocationCall) {
        return nullptr;
    }
    return std::realloc(buffer, bytes);
}

int main(int argc, char**) {
    if (argc == 2) {
        ReallocationFailureRetry();
    } else {
        FrameAdmission();
        ReallocationFailureRetry();
        BaselineAlias();
    }
    std::puts("actual SDK demux boundaries passed: native length admission, realloc failure/retry, baseline alias");
    return 0;
}
