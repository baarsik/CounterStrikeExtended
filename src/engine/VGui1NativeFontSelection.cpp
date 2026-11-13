#include "VGui1NativeFontSelection.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"
#include "../utils/vgui/lib-src/vgui/posix/VGui1FontServices.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace cse_vgui1_selection {
namespace {
constexpr vgui::Scheme::SchemeFont DefaultSlots[] = {vgui::Scheme::sf_primary1, vgui::Scheme::sf_primary2,
    vgui::Scheme::sf_primary3, vgui::Scheme::sf_secondary1};
static_assert(sizeof(DefaultSlots) / sizeof(DefaultSlots[0]) == SchemeFontCount, "Actual Scheme slot membership");

bool Complete(const OwnerV1& owner) noexcept {
    return owner.structBytes == sizeof(owner) && owner.version == OwnerVersion && owner.context
        && owner.ownerThread && owner.pinModule && owner.unpinModule && owner.enterUse && owner.leaveUse;
}
Result Translate(CseVgui1FontResultV1 result) noexcept {
    switch (result) {
    case CseVgui1FontResultV1::Ready:
        return Result::Ready;
    case CseVgui1FontResultV1::PendingResources:
    case CseVgui1FontResultV1::PendingRealization:
        return Result::Pending;
    case CseVgui1FontResultV1::InvalidHandle:
        return Result::InvalidHandle;
    case CseVgui1FontResultV1::ReleaseFailed:
        return Result::ReleaseFailed;
    default:
        return Result::Unavailable;
    }
}
CseVgui1FontStatusV1 StatusRecord() noexcept {
    return {sizeof(CseVgui1FontStatusV1), CseVgui1FontStateV1::PendingResources, 0, 0, 0};
}
}

Selection::~Selection() noexcept {
    if (held_) {
        std::fputs("VGUI1 font selection destroyed before successful retirement\n", stderr);
        std::fflush(stderr);
        std::abort();
    }
}

Diagnostics Selection::Inspect() const noexcept {
    return {state_, held_, atlasValid_, platform_, id_, catalog_, renderer_, serviceResult_};
}

Result Selection::DropCapture(Result reason) noexcept {
    state_ = State::Selected;
    const Result retired = Retire();
    return retired == Result::Retired ? reason : retired;
}

Result Selection::Capture(vgui::Font* font, const OwnerV1& owner) noexcept {
    return CaptureImpl(font, nullptr, 0, owner);
}

Result Selection::CaptureImpl(vgui::Font* font, vgui::Scheme* scheme, std::size_t slot,
    const OwnerV1& owner) noexcept {
    if (state_ == State::ReleasePending) {
        return Result::ReleaseFailed;
    }
    if (state_ != State::Empty && state_ != State::Retired) {
        return Result::Busy;
    }
    if ((!font && !scheme) || (scheme && slot >= SchemeFontCount) || !Complete(owner)) {
        return Result::InvalidArgument;
    }
    if (!owner.ownerThread(owner.context)) {
        return Result::WrongThread;
    }
    owner_ = owner;
    state_ = State::Capturing;
    if (!owner_.pinModule(owner_.context, &moduleToken_)) {
        owner_ = {};
        state_ = State::Empty;
        return Result::Unavailable;
    }
    held_ = true;
    atlasValid_ = false;
    if (!owner_.enterUse(owner_.context, moduleToken_)) {
        return DropCapture(Result::Busy);
    }
    if (scheme) {
        font = scheme->getFont(DefaultSlots[slot]);
    }
    if (!font) {
        owner_.leaveUse(owner_.context, moduleToken_);
        return DropCapture(Result::InvalidHandle);
    }
    platform_ = font->getPlat();
    id_ = font->getId();
    auto status = StatusRecord();
    const auto queried = CseVgui1FontQueryV1(platform_, &status, nullptr, 0);
    serviceResult_ = static_cast<std::int32_t>(queried);
    catalog_ = status.catalogGeneration;
    owner_.leaveUse(owner_.context, moduleToken_);
    if (queried == CseVgui1FontResultV1::InvalidHandle || queried == CseVgui1FontResultV1::InvalidArgument) {
        return DropCapture(Result::InvalidHandle);
    }
    state_ = State::Selected;
    return Result::Captured;
}

Result Selection::BeginUse(std::uint64_t rendererGeneration, PreparedFont& output) noexcept {
    if (state_ == State::ReleasePending) {
        return Result::ReleaseFailed;
    }
    if (state_ != State::Selected) {
        return held_ ? Result::Busy : Result::InvalidHandle;
    }
    if (!owner_.ownerThread(owner_.context)) {
        return Result::WrongThread;
    }
    state_ = State::EnteringUse;
    if (!owner_.enterUse(owner_.context, moduleToken_)) {
        state_ = State::Selected;
        atlasValid_ = false;
        return Result::Busy;
    }
    auto status = StatusRecord();
    const auto prepared = CseVgui1FontPrepareV1(platform_, &status, nullptr, 0);
    serviceResult_ = static_cast<std::int32_t>(prepared);
    Result result = Translate(prepared);
    if (result == Result::Ready && status.state != CseVgui1FontStateV1::Ready) {
        result = Result::Unavailable;
    }
    if (result != Result::Ready || serial_ == (std::numeric_limits<std::uint64_t>::max)()) {
        atlasValid_ = false;
        state_ = State::LeavingUse;
        owner_.leaveUse(owner_.context, moduleToken_);
        state_ = State::Selected;
        return result == Result::Ready ? Result::Exhausted : result;
    }
    if (catalog_ != status.catalogGeneration || renderer_ != rendererGeneration) {
        atlasValid_ = false;
    }
    catalog_ = status.catalogGeneration;
    renderer_ = rendererGeneration;
    ++serial_;
    state_ = State::InUse;
    output = {platform_, id_, catalog_, renderer_, serial_, status.capabilities, !atlasValid_};
    return Result::Ready;
}

Result Selection::MarkAtlasReady(std::uint64_t useSerial) noexcept {
    if (state_ != State::InUse || useSerial != serial_) {
        return Result::InvalidUse;
    }
    if (!owner_.ownerThread(owner_.context)) {
        return Result::WrongThread;
    }
    atlasValid_ = true;
    return Result::Ready;
}

Result Selection::EndUse(std::uint64_t useSerial) noexcept {
    if (state_ != State::InUse || useSerial != serial_) {
        return Result::InvalidUse;
    }
    if (!owner_.ownerThread(owner_.context)) {
        return Result::WrongThread;
    }
    state_ = State::LeavingUse;
    owner_.leaveUse(owner_.context, moduleToken_);
    state_ = State::Selected;
    return Result::Captured;
}

Result Selection::Retire() noexcept {
    if (!held_) {
        return state_ == State::Empty || state_ == State::Retired ? Result::Retired : Result::Busy;
    }
    if (state_ != State::Selected && state_ != State::ReleasePending) {
        return Result::Busy;
    }
    if (!owner_.ownerThread(owner_.context)) {
        return Result::WrongThread;
    }
    atlasValid_ = false;
    state_ = State::Retiring;
    if (!owner_.unpinModule(owner_.context, moduleToken_)) {
        state_ = State::ReleasePending;
        return Result::ReleaseFailed;
    }
    held_ = false;
    owner_ = {};
    platform_ = nullptr;
    id_ = 0;
    catalog_ = 0;
    renderer_ = 0;
    state_ = State::Retired;
    return Result::Retired;
}

Selection* SchemeAdmission::Slot(std::size_t index) noexcept {
    return index < SchemeFontCount ? &slots_[index] : nullptr;
}

Result SchemeAdmission::Retire() noexcept {
    Result result = Result::Retired;
    for (std::size_t index = SchemeFontCount; index != 0; --index) {
        const Result current = slots_[index - 1].Retire();
        if (current != Result::Retired) {
            result = current;
        }
    }
    return result;
}

Result SchemeAdmission::Prepare(vgui::Scheme* scheme, const OwnerV1& owner,
    std::uint64_t rendererGeneration, SchemeReport& report) noexcept {
    for (auto& slot : slots_) {
        const auto state = slot.Inspect();
        if (state.moduleHeld) {
            return state.state == State::ReleasePending ? Result::ReleaseFailed : Result::Busy;
        }
    }
    if (!scheme) {
        return Result::InvalidArgument;
    }
    SchemeReport current{};
    PreparedFont prepared[SchemeFontCount]{};
    Result result = Result::Ready;
    std::size_t active = 0;
    for (std::size_t index = 0; index < SchemeFontCount; ++index) {
        current.failedSlot = static_cast<std::uint32_t>(index);
        result = slots_[index].CaptureImpl(nullptr, scheme, index, owner);
        if (result != Result::Captured) {
            break;
        }
        result = slots_[index].BeginUse(rendererGeneration, prepared[index]);
        current.serviceResult = slots_[index].Inspect().serviceResult;
        if (result != Result::Ready) {
            break;
        }
        ++active;
        ++current.preparedCount;
        if (index == 0) {
            current.catalogGeneration = prepared[index].catalogGeneration;
        } else if (current.catalogGeneration != prepared[index].catalogGeneration) {
            result = Result::Unavailable;
            break;
        }
    }
    while (active != 0) {
        --active;
        slots_[active].EndUse(prepared[active].useSerial);
    }
    if (result != Result::Ready) {
        const Result retired = Retire();
        if (retired != Result::Retired) {
            result = retired;
        }
    }
    report = current;
    return result;
}
}
