#pragma once
#include "../../public/native_socket_host.h"

namespace cse::socket_lifetime {
// Explicit cold-admitted executable initialization, before provider constructors.
bool InitializeProcess() noexcept;

class SocketImageOwner {
public:
    SocketImageOwner() = default;
    ~SocketImageOwner() noexcept;
    SocketImageOwner(const SocketImageOwner&) = delete;
    SocketImageOwner& operator=(const SocketImageOwner&) = delete;
    bool Open(void* admittedImage, const char* path) noexcept;
    bool Retire() noexcept;
    bool Active() const noexcept;

private:
    friend bool HostOwns(void*, const void*, std::uint64_t) noexcept;
    friend Status HostCreateUdp(void*, const void*, std::uint64_t, CreateResult*) noexcept;
    friend Status HostCloseUdp(void*, const void*, std::uint64_t, std::int32_t, CloseResult*) noexcept;
    friend Status HostQuiescent(void*, const void*, std::uint64_t) noexcept;
    friend struct Registry;
    SocketImageOwner* next_ = nullptr;
    void* anchor_ = nullptr;
    const void* base_ = nullptr;
    const API* api_ = nullptr;
    std::uint64_t epoch_ = 0;
    std::int32_t pid_ = 0;
    bool bound_ = false;
    bool fault_ = false;
    bool retiring_ = false;
};
} // namespace cse::socket_lifetime
