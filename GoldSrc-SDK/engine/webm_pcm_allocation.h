#ifndef CSE_WEBM_PCM_ALLOCATION_H
#define CSE_WEBM_PCM_ALLOCATION_H

namespace cse {
namespace webm {

// This owned helper is built with exceptions enabled. Its caller retains its
// existing no-EH policy and native unwind metadata for the engine's outer catch.
short* AllocatePcm(unsigned int sampleCount);

}
}

#endif
