#include "module_fixture.h"
#include "NativeClipboardOwner.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <string>

namespace owner = cse_vgui2_clipboard_owner;

namespace {
int ActiveCalls = 0;
bool Immediate = false;
bool CounterOnly = false;
struct Root {
    std::string path;
    void* retained = nullptr;
    bool queued = false;
    bool queueAllowed = true;
    bool retire = true;
    int unloads = 0;
    std::string name;
    void* codeAddress = nullptr;
    static bool Pin(void* context, std::uint64_t* token) {
        auto& self = *static_cast<Root*>(context);
        if (!CounterOnly) {
            self.retained = dlopen(self.path.c_str(), RTLD_NOW | RTLD_LOCAL);
            assert(self.retained);
        }
        *token = 31;
        return true;
    }
    static bool Queue(void* context, std::uint64_t token) {
        auto& self = *static_cast<Root*>(context);
        assert(token == 31);
        if (!self.queueAllowed) {
            return false;
        }
        if (Immediate) {
            assert(dlclose(self.retained) == 0); // Deliberately rejected negative control.
            self.retained = nullptr;
        } else {
            self.queued = true;
        }
        return true;
    }
    static bool Retire(void* context) { return static_cast<Root*>(context)->retire; }
    static void Named(void* context, const unsigned char* data, std::uint64_t bytes) {
        auto& self = *static_cast<Root*>(context);
        self.name.assign(reinterpret_cast<const char*>(data), bytes);
        std::cout << "actual_dylib_named_board=" << self.name << '\n';
    }
    static void Unloaded(void* context, int tag) {
        assert(ActiveCalls == 0 && "final code reference released while component is executing");
        auto& self = *static_cast<Root*>(context);
        ++self.unloads;
        std::cout << "unloaded_tag=" << tag << '\n';
    }
    FixtureHost Services(std::uint64_t epoch) {
        return {this, epoch, Pin, Queue, Retire, Unloaded, Named};
    }
    void Drain(bool observeResidency = false) {
        assert(ActiveCalls == 0 && queued && retained && unloads == 0);
        queued = false;
        void* image = retained;
        retained = nullptr;
        assert(dlclose(image) == 0);
        Dl_info information{};
        const bool mapped = dladdr(codeAddress, &information) != 0;
        std::cout << "post_dlclose_destructors=" << unloads << " mapped=" << mapped << '\n';
        if (observeResidency) {
            assert(unloads == 0 && mapped); // Observation, NEVER successful-unload qualification.
        } else {
            assert(unloads == 1 && !mapped && "closed image code remains mapped");
        }
    }
};

struct Api {
    void* tagAddress;
    decltype(&FixtureTag) tag;
    decltype(&FixtureCreate) create;
    decltype(&FixturePublish) publish;
    decltype(&FixtureBorrow) borrow;
    decltype(&FixtureReadCell) read;
    decltype(&FixtureEnd) end;
    decltype(&FixtureRetry) retry;
    decltype(&FixtureCloseDestroy) close;
    explicit Api(void* image) {
        tagAddress = dlsym(image, "FixtureTag");
        tag = reinterpret_cast<decltype(tag)>(tagAddress);
        create = reinterpret_cast<decltype(create)>(dlsym(image, "FixtureCreate"));
        publish = reinterpret_cast<decltype(publish)>(dlsym(image, "FixturePublish"));
        borrow = reinterpret_cast<decltype(borrow)>(dlsym(image, "FixtureBorrow"));
        read = reinterpret_cast<decltype(read)>(dlsym(image, "FixtureReadCell"));
        end = reinterpret_cast<decltype(end)>(dlsym(image, "FixtureEnd"));
        retry = reinterpret_cast<decltype(retry)>(dlsym(image, "FixtureRetry"));
        close = reinterpret_cast<decltype(close)>(dlsym(image, "FixtureCloseDestroy"));
        assert(tag && create && publish && borrow && read && end && retry && close);
    }
};

template<class Function, class... Arguments>
int Call(Function function, Arguments... arguments) {
    ++ActiveCalls;
    const int result = function(arguments...);
    --ActiveCalls;
    return result;
}

int Code(owner::Status status) { return static_cast<int>(status); }
}

int main(int argc, char** argv) {
    assert(argc == 3 || argc == 4);
    Immediate = argc == 4 && std::strcmp(argv[3], "unsafe-immediate") == 0;
    CounterOnly = argc == 4 && std::strcmp(argv[3], "counter-only") == 0;
    const bool observeResidency = argc == 4 && std::strcmp(argv[3], "actual-mac-observe") == 0;
    const bool actualMac = observeResidency || (argc == 4 && std::strcmp(argv[3], "actual-mac") == 0);
    Root first;
    first.path = argv[1];
    Root second;
    second.path = argv[2];
    void* image1 = dlopen(first.path.c_str(), RTLD_NOW | RTLD_LOCAL);
    void* image2 = dlopen(second.path.c_str(), RTLD_NOW | RTLD_LOCAL);
    assert(image1 && image2);
    Api a(image1);
    Api b(image2);
    first.codeAddress = a.tagAddress;
    second.codeAddress = b.tagAddress;
    assert(Call(a.tag) == 1 && Call(b.tag) == 2);
    void* object1 = nullptr;
    void* object2 = nullptr;
    auto host1 = first.Services(101);
    auto host2 = second.Services(202);
    assert(Call(a.create, &host1, &object1) == Code(owner::Status::Ok));
    assert(Call(b.create, &host2, &object2) == Code(owner::Status::Ok));
    assert(Call(a.publish, object1) == Code(owner::Status::PrivateOnly));
    assert(Call(b.publish, object2) == Code(owner::Status::PrivateOnly));
    assert(Call(a.borrow, object1) == Code(owner::Status::Ok));
    assert(Call(b.borrow, object2) == Code(owner::Status::Ok));
    assert(dlclose(image1) == 0 && dlclose(image2) == 0);
    assert(first.unloads == 0 && second.unloads == 0 && "counter-only pin did not retain real images");
    assert(Call(a.read, object1) == 'L' && Call(b.read, object2) == 'L');
    assert(Call(a.close, object1) == Code(owner::Status::Busy));
    if (actualMac) {
        assert(!first.name.empty() && !second.name.empty() && first.name != second.name);
        assert(Call(a.end, object1) == Code(owner::Status::Ok));
        first.queueAllowed = false;
        assert(Call(a.close, object1) == Code(owner::Status::ReleaseFailed) && first.unloads == 0);
        first.queueAllowed = true;
        assert(Call(a.close, object1) == Code(owner::Status::Ok) && first.unloads == 0);
        first.Drain(observeResidency);
        assert(second.unloads == 0 && Call(b.read, object2) == 'L');
        assert(Call(b.end, object2) == Code(owner::Status::Ok));
        assert(Call(b.close, object2) == Code(owner::Status::Ok));
        second.Drain(observeResidency);
        if (observeResidency) {
            std::cout << "OBSERVATION ONLY: actual ObjC dylibs retained after local close/ref drain; unload unqualified\n";
            std::cout.flush();
            // Their deliberately installed TEST destructor markers retain host
            // stack pointers. Production has no such static callbacks. Bypass
            // process-exit marker execution after both local owners are closed.
            std::_Exit(0);
        }
        std::cout << "actual Mac backend diagnostic bundle unmapping/post-return host drain PASS\n";
        return 0;
    }
    first.retire = false;
    assert(Call(a.end, object1) == Code(owner::Status::ReleaseFailed));
    assert(Call(a.close, object1) == Code(owner::Status::ReleaseFailed) && first.unloads == 0);
    first.retire = true;
    assert(Call(a.retry, object1) == Code(owner::Status::Ok));
    first.queueAllowed = false;
    assert(Call(a.close, object1) == Code(owner::Status::ReleaseFailed) && first.unloads == 0);
    first.queueAllowed = true;
    assert(Call(a.close, object1) == Code(owner::Status::Ok));
    assert(first.queued && first.unloads == 0);
    first.Drain();
    assert(second.unloads == 0 && Call(b.read, object2) == 'L');
    assert(Call(b.end, object2) == Code(owner::Status::Ok));
    assert(Call(b.close, object2) == Code(owner::Status::Ok));
    second.Drain();
    std::cout << "genuine independent image refs, failed retirement and post-return host unload PASS\n";
}
