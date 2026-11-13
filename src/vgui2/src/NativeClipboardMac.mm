#include "NativeClipboardMac.h"

#import <AppKit/AppKit.h>
#include <new>
#include <pthread.h>

namespace cse_vgui2_clipboard_owner {
namespace core = cse_vgui2_clipboard;
namespace {

constexpr char PrivateTypeBytes[] = "org.cse.clipboard.snapshot-v1";

struct MacState {
    DependenciesV1 dependencies{};
    NSPasteboard* board = nil;
    NSString* name = nil;
    NSString* privateType = nil;
    NSData* privateData = nil;
    NSData* publicData = nil;
    NSArray<NSPasteboardType>* types = nil;
    std::size_t activeReads = 0;
    bool namedIdentityAccepted = false;
    bool releaseSubmitted = false;
};

struct MacRead {
    NSData* bytes = nil;
};

bool MainThread() noexcept {
    return pthread_main_np() != 0;
}

bool StandardName(NSString* name) {
    return [name isEqualToString:NSPasteboardNameGeneral] || [name isEqualToString:NSPasteboardNameFont]
        || [name isEqualToString:NSPasteboardNameRuler] || [name isEqualToString:NSPasteboardNameFind]
        || [name isEqualToString:NSPasteboardNameDrag];
}

Status Guard(MacState& state) {
    if (!MainThread()) {
        return Status::WrongThread;
    }
    if (!state.board || !state.name || state.releaseSubmitted || StandardName(state.name)
        || ![state.board.name isEqualToString:state.name]) {
        return Status::Unavailable;
    }
    return Status::Ok;
}

Status Open(void* context) noexcept {
    auto& state = *static_cast<MacState*>(context);
    if (!MainThread()) {
        return Status::WrongThread;
    }
    @autoreleasepool {
        @try {
            // AppKit may retain a declared type. A copied Foundation string
            // must own its bytes independently of an unloadable image's
            // Objective-C constant-string object or literal storage.
            state.privateType = [[NSString alloc] initWithBytes:PrivateTypeBytes length:sizeof(PrivateTypeBytes) - 1
                encoding:NSASCIIStringEncoding];
            if (!state.privateType) {
                return Status::NoMemory;
            }
            // This is the ONLY host binding factory. No caller-supplied name,
            // general pasteboard, filtering service, observer or lazy provider.
            state.board = [NSPasteboard pasteboardWithUniqueName];
            state.name = [state.board.name copy];
            if (!state.name.length || StandardName(state.name)) {
                return Status::Unavailable;
            }
            state.namedIdentityAccepted = true;
            if (state.dependencies.diagnostic) {
                NSData* nameBytes = [state.name dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
                if (!nameBytes) {
                    return Status::Unavailable;
                }
                state.dependencies.diagnostic(state.dependencies.context, "named-board",
                    {static_cast<const unsigned char*>(nameBytes.bytes), nameBytes.length});
            }
            return Status::Ok;
        } @catch (NSException*) {
            return Status::Unavailable;
        }
    }
}

Status Read(void* context, HostRead* output) noexcept {
    auto& state = *static_cast<MacState*>(context);
    if (!MainThread()) {
        return Status::WrongThread;
    }
    @autoreleasepool {
        @try {
            const auto admitted = Guard(state);
            if (admitted != Status::Ok) {
                return admitted;
            }
            const NSInteger before = state.board.changeCount;
            NSArray<NSPasteboardItem*>* items = state.board.pasteboardItems;
            // No implicit whole-board text/newline aggregation, first-item drop
            // or external UTF8/raw8 synthesis policy exists in this component.
            if (items.count != 1) {
                return Status::MissingFormat;
            }
            NSData* source = [items[0] dataForType:state.privateType];
            if (!source) {
                return Status::MissingFormat;
            }
            NSData* copied = [[NSData alloc] initWithBytes:source.bytes length:source.length];
            if (!copied) {
                return Status::NoMemory;
            }
            // Finish every potentially throwing host/object observation before
            // publishing a token or incrementing activeReads. An exception after
            // that transfer must never strand an unreachable retained borrow.
            const auto* bytes = static_cast<const unsigned char*>(copied.bytes);
            const NSUInteger length = copied.length;
            const NSInteger after = state.board.changeCount;
            auto* lease = new (std::nothrow) MacRead;
            if (!lease) {
                return Status::NoMemory;
            }
            lease->bytes = copied;
            ++state.activeReads;
            *output = {{bytes, length}, lease, static_cast<std::int64_t>(before), static_cast<std::int64_t>(after)};
            return Status::Ok;
        } @catch (NSException*) {
            return Status::Unavailable;
        }
    }
}

bool ReleaseRead(void* context, void* token) noexcept {
    auto& state = *static_cast<MacState*>(context);
    if (!MainThread() || !token || state.activeReads == 0) {
        return false;
    }
    // Only our copied, plain NSData is retained. No provider, no-copy destructor
    // or user Objective-C object can execute Root code during this local release.
    delete static_cast<MacRead*>(token);
    --state.activeReads;
    return true;
}

Status Stage(void* context, core::ByteSpan privateBytes, core::ByteSpan publicBytes, bool hasPublic) noexcept {
    auto& state = *static_cast<MacState*>(context);
    if (!MainThread()) {
        return Status::WrongThread;
    }
    @autoreleasepool {
        @try {
            const auto admitted = Guard(state);
            if (admitted != Status::Ok) {
                return admitted;
            }
            NSData* privateData = [[NSData alloc] initWithBytes:privateBytes.data length:privateBytes.storageBytes];
            NSData* publicData = hasPublic
                ? [[NSData alloc] initWithBytes:publicBytes.data length:publicBytes.storageBytes] : nil;
            NSArray<NSPasteboardType>* types = hasPublic
                ? @[state.privateType, NSPasteboardTypeString] : @[state.privateType];
            if (!privateData || (hasPublic && !publicData) || !types) {
                return Status::NoMemory;
            }
            state.privateData = privateData;
            state.publicData = publicData;
            state.types = types;
            return Status::Ok;
        } @catch (NSException*) {
            return Status::Unavailable; // No host contents have been touched.
        }
    }
}

Status Prepare(void* context, std::int64_t* change) noexcept {
    auto& state = *static_cast<MacState*>(context);
    @autoreleasepool {
        @try {
            const auto admitted = Guard(state);
            if (admitted != Status::Ok || !state.privateData || !state.types) {
                return admitted == Status::Ok ? Status::Unavailable : admitted;
            }
            *change = static_cast<std::int64_t>([state.board
                prepareForNewContentsWithOptions:NSPasteboardContentsCurrentHostOnly]);
            return Status::Ok;
        } @catch (NSException*) {
            return Status::StateUncertain;
        }
    }
}

Status AddTypes(void* context, bool hasPublic, std::int64_t* change) noexcept {
    auto& state = *static_cast<MacState*>(context);
    @autoreleasepool {
        @try {
            const auto admitted = Guard(state);
            if (admitted != Status::Ok) {
                return admitted;
            }
            if (state.board.changeCount != *change || hasPublic != (state.publicData != nil)) {
                return Status::OwnershipChanged;
            }
            const NSInteger declared = [state.board addTypes:state.types owner:nil];
            if (declared == 0) {
                return Status::HostFailure;
            }
            *change = static_cast<std::int64_t>(declared);
            return state.board.changeCount == declared ? Status::Ok : Status::OwnershipChanged;
        } @catch (NSException*) {
            return Status::StateUncertain;
        }
    }
}

Status Set(MacState& state, NSData* bytes, NSPasteboardType type, core::ByteSpan original,
    std::int64_t change) noexcept {
    @autoreleasepool {
        @try {
            const auto admitted = Guard(state);
            if (admitted != Status::Ok) {
                return admitted;
            }
            if (!bytes || bytes.length != original.storageBytes) {
                return Status::HostFailure;
            }
            if (state.board.changeCount != change || ![state.board setData:bytes forType:type]) {
                return Status::OwnershipChanged;
            }
            return state.board.changeCount == change ? Status::Ok : Status::OwnershipChanged;
        } @catch (NSException*) {
            return Status::StateUncertain;
        }
    }
}

Status SetPrivate(void* context, core::ByteSpan bytes, std::int64_t change) noexcept {
    auto& state = *static_cast<MacState*>(context);
    return Set(state, state.privateData, state.privateType, bytes, change);
}

Status SetPublic(void* context, core::ByteSpan bytes, std::int64_t change) noexcept {
    auto& state = *static_cast<MacState*>(context);
    return Set(state, state.publicData, NSPasteboardTypeString, bytes, change);
}

Status Close(void* context) noexcept {
    auto& state = *static_cast<MacState*>(context);
    if (!MainThread()) {
        return Status::WrongThread;
    }
    if (state.activeReads != 0) {
        return Status::Busy;
    }
    @autoreleasepool {
        @try {
            if (state.board && state.namedIdentityAccepted && !state.releaseSubmitted) {
                // oneway void: submission only, NEVER a server deletion ack.
                [state.board releaseGlobally];
                state.releaseSubmitted = true;
            }
            // An unadmitted identity is released locally only. Never attempt a
            // global destructive operation on an unknown/standard board. Local
            // close does not claim deletion of any server-side named object.
            delete &state;
            return Status::Ok;
        } @catch (NSException*) {
            return Status::StateUncertain;
        }
    }
}
}

Status CreateNamedFixtureOwner(const DependenciesV1& dependencies, Owner& owner) noexcept {
    if (owner.HoldsRoot()) {
        return Status::Busy;
    }
    auto* state = new (std::nothrow) MacState;
    if (!state) {
        return Status::NoMemory;
    }
    state->dependencies = dependencies;
    const BackendV1 backend{sizeof(BackendV1), ContractVersion, state, Open, Read, ReleaseRead, Stage,
        Prepare, AddTypes, SetPrivate, SetPublic, Close};
    const auto result = owner.Open(dependencies, backend);
    if (!owner.HoldsRoot()) {
        delete state; // No retained context or host operation occurred.
    }
    return result;
}

} // namespace cse_vgui2_clipboard_owner
