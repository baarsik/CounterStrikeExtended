extern "C" int CseFixtureLeafValue();

namespace {
struct OwnedState {
    int value = CseFixtureLeafValue();

    ~OwnedState() {
        value = CseFixtureLeafValue();
    }
};
OwnedState g_State;
}

extern "C" int CseFixtureConsumerValue() {
    return g_State.value;
}
