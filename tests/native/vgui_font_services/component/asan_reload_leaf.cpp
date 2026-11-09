#if defined(CSE_ASAN_RELOAD_VIRTUAL)
class CseOwnedLeafInterface {
public:
    virtual ~CseOwnedLeafInterface();
    virtual int Value();
};

CseOwnedLeafInterface::~CseOwnedLeafInterface() = default;

int CseOwnedLeafInterface::Value() {
    return 17;
}

CseOwnedLeafInterface g_OwnedLeafInterface;
#endif

namespace {
int g_OwnedValues[4]{};
}

extern "C" int CseFixtureLeafValue() {
    g_OwnedValues[0] = 17;
#if defined(CSE_ASAN_RELOAD_VIRTUAL)
    g_OwnedValues[0] = g_OwnedLeafInterface.Value();
#endif
    return g_OwnedValues[0];
}
