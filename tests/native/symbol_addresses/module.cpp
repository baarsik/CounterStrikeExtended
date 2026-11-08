#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" FIXTURE_EXPORT int CseSymbolFixtureFunction(int value) {
    return value + 1;
}

class OwnedSymbolFixture {
public:
    static FIXTURE_EXPORT int Think(int value);
};

int OwnedSymbolFixture::Think(int value) {
    return value + 2;
}
