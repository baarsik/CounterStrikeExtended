#pragma once
#include "engine/native_socket_lifetime.h"
#include "launcher/native/SocketImageOwner.h"
#include <cassert>
#include <dlfcn.h>

// Component fixture adapter around the real producer/host. No cold/unmap claim for a linked provider.
class LinkedSocketOwner {
public:
    LinkedSocketOwner() {
        assert(cse::socket_lifetime::InitializeProcess());
        Dl_info physical{};
        assert(dladdr(reinterpret_cast<const void*>(&cse::socket_lifetime::CreateUdp), &physical));
        image_ = dlopen(physical.dli_fname, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
        assert(image_ && owner_.Open(image_, physical.dli_fname));
    }
    ~LinkedSocketOwner() {
        assert(owner_.Retire());
        assert(dlclose(image_) == 0);
    }
private:
    cse::socket_lifetime::SocketImageOwner owner_;
    void* image_ = nullptr;
};
