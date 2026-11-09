#include "webm_pcm_allocation.h"

extern unsigned int helperInnerDestructors;

namespace {

struct InnerLifetime {
    ~InnerLifetime() {
        ++helperInnerDestructors;
    }
};

}

short* InvokePcmFromNoEh(unsigned int count) {
    InnerLifetime lifetime;
    return cse::webm::AllocatePcm(count);
}
