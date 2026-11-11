#include "VGui1HostImageOwner.h"
#include "../../public/native_module.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace cse_vgui1_host {
ImageOwner::ImageOwner() noexcept : thread_(pthread_self()) {}

bool ImageOwner::OnOwnerThread() const noexcept {
    return pthread_equal(thread_, pthread_self()) != 0;
}

bool ImageOwner::ReadUuid(const void* base, unsigned char* output) noexcept {
    // Only an OS-loaded image obtained from a real handle/dladdr is inspected.
    // This is NOT a reader/validator for arbitrary on-disk Mach-O input.
    mach_header_64 header{};
    std::memcpy(&header, base, sizeof(header));
    if (header.magic != MH_MAGIC_64 || header.filetype != MH_DYLIB
        || header.ncmds > header.sizeofcmds / sizeof(load_command)) {
        return false;
    }
    const auto* commands = static_cast<const unsigned char*>(base) + sizeof(header);
    std::size_t remaining = header.sizeofcmds;
    bool found = false;
    for (std::uint32_t index = 0; index < header.ncmds; ++index) {
        if (remaining < sizeof(load_command)) {
            return false;
        }
        load_command command{};
        std::memcpy(&command, commands, sizeof(command));
        if (command.cmdsize < sizeof(command) || command.cmdsize > remaining) {
            return false;
        }
        if (command.cmd == LC_UUID) {
            if (found || command.cmdsize != sizeof(uuid_command)) {
                return false;
            }
            uuid_command uuid{};
            std::memcpy(&uuid, commands, sizeof(uuid));
            std::memcpy(output, uuid.uuid, UuidBytes);
            found = true;
        }
        commands += command.cmdsize;
        remaining -= command.cmdsize;
    }
    return found && remaining == 0;
}

bool ImageOwner::ReadIdentity(void* reference, const char* path, Identity& output) noexcept {
    if (!reference || !path || path[0] != '/' || strnlen(path, sizeof(output.path)) == sizeof(output.path)) {
        return false;
    }
    const void* factory = cse_platform::GetSymbol(reference, cse_platform::InterfaceFactorySymbol);
    Dl_info actual{};
    if (!factory || !dladdr(factory, &actual) || !actual.dli_fbase || !actual.dli_fname
        || actual.dli_fbase == _NSGetMachExecuteHeader()) {
        return false;
    }
    char actualPath[PATH_MAX]{};
    if (!realpath(path, output.path) || !realpath(actual.dli_fname, actualPath)
        || std::strcmp(output.path, actualPath) != 0 || !ReadUuid(actual.dli_fbase, output.uuid)) {
        return false;
    }
    output.base = actual.dli_fbase;
    output.factory = factory;
    return true;
}

bool ImageOwner::SameImage(void* reference) const noexcept {
    if (!reference || reference != directHandle_) {
        return false;
    }
    const void* factory = cse_platform::GetSymbol(reference, cse_platform::InterfaceFactorySymbol);
    Dl_info actual{};
    unsigned char uuid[UuidBytes]{};
    return factory && factory == identity_.factory && dladdr(factory, &actual)
        && actual.dli_fbase == identity_.base && ReadUuid(actual.dli_fbase, uuid)
        && std::memcmp(uuid, identity_.uuid, sizeof(uuid)) == 0;
}

Result ImageOwner::Open(const AdmittedImageV1& image, AnchorMode mode, void* initial) noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    if (busy_ || state_ != State::Closed || anchor_ || pending_ || depth_) {
        return Result::Busy;
    }
    if (image.structBytes != sizeof(image) || image.version != AdmittedImageVersion || image.epoch == 0
        || !image.borrowedReference || !image.absolutePath
        || (mode != AnchorMode::AdoptInitialReference && mode != AnchorMode::FirstTransferredReference)
        || (mode == AnchorMode::AdoptInitialReference ? !initial : initial != nullptr)) {
        return Result::InvalidArgument;
    }
    if (image.epoch <= lastEpoch_) {
        return Result::InvalidEpoch;
    }
    Dl_info callback{};
    // Inserted dylibs can precede MH_EXECUTE in dyld's image enumeration on ARM64.
    if (!dladdr(reinterpret_cast<const void*>(&Accept), &callback)
        || callback.dli_fbase != _NSGetMachExecuteHeader()) {
        return Result::InvalidCallbackImage;
    }
    Identity identity{};
    if (!ReadIdentity(image.borrowedReference, image.absolutePath, identity)) {
        return Result::InvalidImage;
    }
    if (initial && initial != image.borrowedReference) {
        return Result::InvalidImage;
    }
    // Darwin dlsym also searches dependencies. A foreign proxy can resolve
    // root's factory, so symbol/dladdr identity alone does NOT identify a handle.
    // The SDK documents same-path loads returning the same handle. Only the
    // normal direct handles used by NativeModule are this component's contract;
    // differently tagged RTLD_FIRST handles are not silently reinterpreted.
    void* validation = dlopen(identity.path, RTLD_NOW | RTLD_NOLOAD | RTLD_LOCAL);
    if (!validation) {
        return Result::InvalidImage;
    }
    const bool direct = validation == image.borrowedReference;
    busy_ = true;
    const bool validationReleased = ReleaseChecked(validation);
    busy_ = false;
    if (!validationReleased) {
        identity_ = identity;
        directHandle_ = validation;
        anchor_ = validation;
        epoch_ = image.epoch;
        lastEpoch_ = image.epoch;
        maximumRetained_ = 0;
        accepted_ = 0;
        balanced_ = 0;
        state_ = State::Closing;
        releaseFailed_ = true;
        ObserveRetained();
        return Result::ReleaseFailed;
    }
    if (!direct) {
        return Result::InvalidImage;
    }
    identity_ = identity;
    directHandle_ = image.borrowedReference;
    anchor_ = initial;
    epoch_ = image.epoch;
    lastEpoch_ = image.epoch;
    maximumRetained_ = 0;
    accepted_ = 0;
    balanced_ = 0;
    releaseCode_ = 0;
    releaseErrno_ = 0;
    releaseDiagnostic_[0] = '\0';
    releaseFailed_ = false;
    state_ = State::Open;
    ObserveRetained();
    return Result::Ready;
}

HostReleasesV2 ImageOwner::Contract() noexcept {
    return {sizeof(HostReleasesV2), HostReleasesVersion, this, Accept};
}

Result ImageOwner::EnterRoot(void* retained) noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    if (busy_) {
        return Result::Busy;
    }
    if (releaseFailed_ || state_ == State::Closing) {
        return Result::ReleaseFailed;
    }
    if (state_ != State::Open) {
        return Result::Closed;
    }
    if (depth_ == (std::numeric_limits<std::uint32_t>::max)()) {
        return Result::Exhausted;
    }
    if (retained && !SameImage(retained)) {
        return Result::InvalidImage;
    }
    if (!anchor_ && !retained && !entryProof_) {
        return Result::MissingAnchor;
    }
    if (depth_ == 0) {
        entryProof_ = retained;
    }
    ++depth_;
    return Result::Ready;
}

Result ImageOwner::LeaveRoot() noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    if (busy_) {
        return Result::Busy;
    }
    if (depth_ == 0) {
        return Result::InvalidArgument;
    }
    --depth_;
    if (depth_ == 0) {
        entryProof_ = nullptr;
    }
    return Result::Ready;
}

void ImageOwner::ObserveRetained() noexcept {
    const std::uint32_t retained = (anchor_ ? 1U : 0U) + (pending_ ? 1U : 0U);
    if (retained > maximumRetained_) {
        maximumRetained_ = retained;
    }
}

bool ImageOwner::ReleaseChecked(void* reference) noexcept {
    // NativeModule::UnloadModule intentionally discards the OS result. This
    // local boundary must NOT erase a reference on a reported release failure.
    dlerror();
    releaseCode_ = dlclose(reference);
    releaseErrno_ = errno;
    releaseDiagnostic_[0] = '\0';
    if (releaseCode_ == 0) {
        return true;
    }
    const char* text = dlerror();
    if (!text) {
        text = "loader reported failure without error text";
    }
    const std::size_t bytes = strnlen(text, sizeof(releaseDiagnostic_) - 1);
    std::memcpy(releaseDiagnostic_, text, bytes);
    releaseDiagnostic_[bytes] = '\0';
    return false;
}

Result ImageOwner::BalancePending() noexcept {
    busy_ = true;
    const bool released = ReleaseChecked(pending_);
    busy_ = false;
    if (!released) {
        releaseFailed_ = true;
        return Result::ReleaseFailed;
    }
    pending_ = nullptr;
    ++balanced_; // Acceptance reserved this count before taking ownership.
    releaseFailed_ = false;
    return Result::Ready;
}

bool ImageOwner::Accept(void* value, void* reference) noexcept {
    return value && static_cast<ImageOwner*>(value)->AcceptReference(reference);
}

bool ImageOwner::AcceptReference(void* reference) noexcept {
    if (!OnOwnerThread()) {
        return false;
    }
    if (busy_ || state_ != State::Open || !depth_ || pending_ || releaseFailed_ || !reference
        || (!anchor_ && !entryProof_) || accepted_ == (std::numeric_limits<std::uint64_t>::max)()
        || (anchor_ && balanced_ == (std::numeric_limits<std::uint64_t>::max)()) || !SameImage(reference)) {
        return false;
    }
    ++accepted_;
    if (!anchor_) {
        anchor_ = reference;
        ObserveRetained();
        return true;
    }
    pending_ = reference;
    ObserveRetained();
    BalancePending();
    return true; // Includes post-accept failure: the pending slot owns the reference.
}

Result ImageOwner::RetryBalance() noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    if (busy_) {
        return Result::Busy;
    }
    if (pending_) {
        return BalancePending();
    }
    return releaseFailed_ ? Result::ReleaseFailed : state_ == State::Closed ? Result::Closed : Result::Ready;
}

Result ImageOwner::Close() noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    if (busy_ || depth_) {
        return Result::Busy;
    }
    if (state_ == State::Closed) {
        return Result::Closed;
    }
    state_ = State::Closing;
    if (pending_ && BalancePending() != Result::Ready) {
        return Result::ReleaseFailed;
    }
    if (anchor_) {
        busy_ = true;
        const bool released = ReleaseChecked(anchor_);
        busy_ = false;
        if (!released) {
            releaseFailed_ = true;
            return Result::ReleaseFailed;
        }
        anchor_ = nullptr;
    }
    releaseFailed_ = false;
    entryProof_ = nullptr;
    identity_ = {};
    directHandle_ = nullptr;
    state_ = State::Closed;
    return Result::Closed;
}

Result ImageOwner::Inspect(Statistics& output) const noexcept {
    if (!OnOwnerThread()) {
        return Result::WrongThread;
    }
    Statistics value{state_, depth_, (anchor_ ? 1U : 0U) + (pending_ ? 1U : 0U), maximumRetained_,
        epoch_, accepted_, balanced_, releaseFailed_, releaseCode_, releaseErrno_, {}};
    std::memcpy(value.nativeError, releaseDiagnostic_, sizeof(value.nativeError));
    output = value;
    return Result::Ready;
}
}
