#include "localization_state.h"
#include <cassert>
#include <string>

using namespace cse::localization;

struct Fixture final : StateBackend {
    std::string overrideLanguage;
    std::string steam = "english";
    std::string disk = "english";
    std::string active = "english";
    bool canPrepare = true;
    bool canPersist = true;
    int prepares = 0;
    int commits = 0;
    int discards = 0;
    std::string Resolve(const std::string& preference) override {
        return cse::interface_language::Resolve(steam.c_str(), overrideLanguage.c_str(), preference.c_str());
    }
    bool Prepare(const std::string&, std::string& error) override {
        ++prepares;
        error = canPrepare ? "" : "missing catalogue";
        return canPrepare;
    }
    bool Persist(const std::string& preference, std::string& error) override {
        if (!canPersist) {
            error = "preference denied";
            return false;
        }
        disk = preference;
        return true;
    }
    void Commit(const std::string& language, std::uint64_t) override {
        active = language;
        ++commits;
    }
    void Discard() override {
        ++discards;
    }
};

static RequestState Result(State& state, std::uint64_t serial) {
    RequestResult result{sizeof(RequestResult), RequestState::Pending, 0};
    assert(state.Result(serial, &result));
    return result.state;
}

int main() {
    State state;
    state.Initialize("english", "english");
    Fixture backend;
    assert(state.Request("../russian", true) == InvalidRequest);
    assert(state.Request("unbundled", true) == InvalidRequest);
    assert(state.Revision() == 1);
    const auto first = state.Request("russian", true);
    assert(first && Result(state, first) == RequestState::Pending);
    assert(state.Language() == "english" && backend.disk == "english");
    const auto second = state.Request("polish", false);
    assert(Result(state, first) == RequestState::Superseded);
    state.Process(backend);
    assert(Result(state, second) == RequestState::Applied);
    assert(state.Language() == "polish" && state.Revision() == 2);
    assert(backend.disk == "english" && backend.active == "polish");
    backend.canPrepare = false;
    const auto failed = state.Request("russian", true);
    state.Process(backend);
    assert(Result(state, failed) == RequestState::Failed);
    assert(state.Language() == "polish" && state.Revision() == 2 && backend.disk == "english");
    assert(std::string(state.LastError()) == "missing catalogue");
    backend.canPrepare = true;
    backend.canPersist = false;
    const auto denied = state.Request("russian", true);
    state.Process(backend);
    assert(Result(state, denied) == RequestState::Failed);
    assert(state.Language() == "polish" && backend.commits == 1 && state.Revision() == 2);
    backend.canPersist = true;
    backend.overrideLanguage = "polish";
    const int previousPrepares = backend.prepares;
    const auto same = state.Request("russian", true);
    state.Process(backend);
    assert(Result(state, same) == RequestState::Applied);
    assert(state.Language() == "polish" && state.Preference() == "russian" && state.Revision() == 2);
    assert(backend.prepares == previousPrepares && backend.disk == "russian");
    backend.overrideLanguage.clear();
    backend.steam = "german";
    const auto automatic = state.Request("", false);
    state.Process(backend);
    assert(Result(state, automatic) == RequestState::Applied && state.Language() == "german");
    RequestResult undersized{0, RequestState::Pending, 0};
    assert(!state.Result(automatic, &undersized));
    assert(!state.Result(InvalidRequest, nullptr));
    for (int index = 0; index < 40; ++index) {
        assert(state.Request(index % 2 ? "english" : "polish", false));
    }
    RequestResult retired{sizeof(RequestResult), RequestState::Pending, 0};
    assert(!state.Result(first, &retired));
    state.Process(backend);
    assert(state.Language() == "english");
}
