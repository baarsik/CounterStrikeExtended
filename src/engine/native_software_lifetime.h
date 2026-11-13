#pragma once
#include "../public/native_software_lifetime.h"

namespace cse::software_lifetime {
// Private provider entry fence. Covers complete public engine calls, including nested callbacks.
enum class EntryKind { Nested, Outermost };
bool Enter(Mode mode, EntryKind kind = EntryKind::Nested) noexcept;
bool EnterCurrent() noexcept;
void Leave() noexcept;
FSteamNetworkingSocketsDebugOutput HostNetworkLog() noexcept;
class Entry {
public:
    Entry() noexcept : entered_(EnterCurrent()) {}
    explicit Entry(Mode mode, EntryKind kind = EntryKind::Nested) noexcept : entered_(Enter(mode, kind)) {}
    ~Entry() noexcept {
        if (entered_) {
            Leave();
        }
    }
    explicit operator bool() const noexcept {
        return entered_;
    }
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

private:
    bool entered_;
};
// Actual sys_dll2 adapter consumes the reviewed staged owners. False retains all image references.
bool RetireResources(Mode mode);
bool RetirementInProgress() noexcept;
bool RetireRemoteAccess() noexcept;
} // namespace cse::software_lifetime
