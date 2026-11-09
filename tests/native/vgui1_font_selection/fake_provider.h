#ifndef CSE_TEST_FONT_SELECTION_PROVIDER_H
#define CSE_TEST_FONT_SELECTION_PROVIDER_H
#include "VGui1FontPlatform.h"

struct ProviderStats {
    int contexts;
    int liveFaces;
    int createdFaces;
    int destroyedFaces;
    int realizations;
    int releases;
};
ProviderStats ProviderSnapshot();
void ProviderFailRelease(int count);
void ProviderMissingSymbol(bool missing);
#endif
