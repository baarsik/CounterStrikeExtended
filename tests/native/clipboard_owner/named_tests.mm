#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include "NativeClipboardMac.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <new>
#include <dlfcn.h>

std::atomic<unsigned int> NothrowAllocations{0};
std::atomic<std::uint64_t> NextEpoch{0};
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ++NothrowAllocations;
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

namespace owner = cse_vgui2_clipboard_owner;
namespace core = cse_vgui2_clipboard;

namespace {
struct Root {
    std::string name;
    int references = 0;
    int queued = 0;
    int allocations = 0;
    static bool Thread(void*) { return true; } // Real backend additionally enforces main thread.
    static bool Pin(void* context, std::uint64_t* token) {
        ++static_cast<Root*>(context)->references;
        *token = 5;
        return true;
    }
    static bool Queue(void* context, std::uint64_t token) {
        assert(token == 5);
        ++static_cast<Root*>(context)->queued;
        return true;
    }
    static void* Allocate(void* context, std::size_t size) {
        void* data = std::malloc(size);
        if (data) {
            ++static_cast<Root*>(context)->allocations;
        }
        return data;
    }
    static void Free(void* context, void* data) {
        if (data) {
            --static_cast<Root*>(context)->allocations;
            std::free(data);
        }
    }
    static owner::Status Encode(void*, core::ByteSpan units, core::ByteBuffer output, std::uint64_t* bytes) {
        @autoreleasepool {
            NSString* text = [[NSString alloc] initWithBytes:units.data length:units.storageBytes
                encoding:NSUTF16LittleEndianStringEncoding];
            NSData* encoded = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
            if (!encoded) {
                return owner::Status::Unrepresentable;
            }
            if (output.data) {
                assert(output.storageBytes == encoded.length);
                std::memcpy(output.data, encoded.bytes, encoded.length);
            }
            *bytes = encoded.length;
            return owner::Status::Ok;
        }
    }
    static void Diagnostic(void* context, const char* event, core::ByteSpan name) {
        assert(std::strcmp(event, "named-board") == 0);
        static_cast<Root*>(context)->name.assign(reinterpret_cast<const char*>(name.data), name.storageBytes);
    }
    owner::DependenciesV1 Dependencies() {
        return {sizeof(owner::DependenciesV1), 1, this, ++NextEpoch, Thread, Pin, Queue,
            Allocate, Free, Encode, Diagnostic};
    }
    void Drain() {
        // Executable-owned root contract for this fixture. Real dylib loader
        // references and post-return release are separately tested.
        references -= queued;
        queued = 0;
        assert(references == 0 && allocations == 0);
    }
};

NSPasteboard* FaultBoard = nil;
IMP OriginalSet = nullptr;
IMP OriginalCount = nullptr;
int CountCalls = 0;
IMP OriginalUnique = nullptr;
IMP OriginalName = nullptr;
IMP OriginalRelease = nullptr;
int RejectedReleases = 0;
int ForbiddenBoardCalls = 0;
IMP OriginalNamed = nullptr;
IMP OriginalTypes = nullptr;
bool ImageBackedType = false;

NSInteger InspectTypes(id object, SEL selector, NSArray<NSPasteboardType>* types, id sourceOwner) {
    assert(object == FaultBoard && sourceOwner == nil);
    for (NSString* type in types) {
        if ([type isEqualToString:@"org.cse.clipboard.snapshot-v1"]) {
            Dl_info information{};
            ImageBackedType = dladdr((__bridge const void*)type, &information) != 0
                || dladdr(type.UTF8String, &information) != 0;
        }
    }
    return reinterpret_cast<NSInteger (*)(id, SEL, NSArray<NSPasteboardType>*, id)>(OriginalTypes)(
        object, selector, types, sourceOwner);
}

void CopiedTypeStorage() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        FaultBoard = [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:root.name.c_str()]];
        Method method = class_getInstanceMethod(object_getClass(FaultBoard), @selector(addTypes:owner:));
        OriginalTypes = method_setImplementation(method, reinterpret_cast<IMP>(InspectTypes));
        const unsigned char units[] = {'Q', 0, 0, 0};
        assert(clipboard.Publish({false, {nullptr, 0}, 0, true, {units, 4}, 4, false, 0})
            == owner::Status::PrivateOnly);
        method_setImplementation(method, OriginalTypes);
        assert(clipboard.Close() == owner::Status::Ok);
        FaultBoard = nil;
    }
    root.Drain();
    if (ImageBackedType) {
        std::cerr << "RED: published private type retains component-image object/bytes\n";
        std::_Exit(76);
    }
}

id ForbidGeneral(id, SEL) {
    ++ForbiddenBoardCalls;
    @throw [NSException exceptionWithName:@"CSEForbiddenBoard" reason:@"General access blocked" userInfo:nil];
}

id GuardNamed(id object, SEL selector, NSPasteboardName name) {
    if ([name isEqualToString:NSPasteboardNameGeneral] || [name isEqualToString:NSPasteboardNameFind]
        || [name isEqualToString:NSPasteboardNameFont] || [name isEqualToString:NSPasteboardNameRuler]
        || [name isEqualToString:NSPasteboardNameDrag]) {
        ++ForbiddenBoardCalls;
        @throw [NSException exceptionWithName:@"CSEForbiddenBoard" reason:@"standard access blocked" userInfo:nil];
    }
    return reinterpret_cast<id (*)(id, SEL, NSPasteboardName)>(OriginalNamed)(object, selector, name);
}
__weak NSData* ObservedData = nil;

BOOL SetThenThrow(id object, SEL selector, NSData* data, NSPasteboardType type) {
    assert(object == FaultBoard);
    ObservedData = data;
    const BOOL written = reinterpret_cast<BOOL (*)(id, SEL, NSData*, NSPasteboardType)>(OriginalSet)(
        object, selector, data, type);
    assert(written);
    @throw [NSException exceptionWithName:@"CSEAuthoredFailure" reason:@"after named publication" userInfo:nil];
}

id UniqueThenThrow(id, SEL) {
    @throw [NSException exceptionWithName:@"CSEAuthoredFailure" reason:@"before creation" userInfo:nil];
}

id CaptureUnique(id object, SEL selector) {
    FaultBoard = reinterpret_cast<id (*)(id, SEL)>(OriginalUnique)(object, selector);
    return FaultBoard;
}

id RejectedName(id object, SEL selector) {
    return object == FaultBoard ? NSPasteboardNameGeneral
        : reinterpret_cast<id (*)(id, SEL)>(OriginalName)(object, selector);
}

void ObserveRejectedRelease(id object, SEL) {
    assert(object == FaultBoard);
    ++RejectedReleases;
}

BOOL ObserveSet(id object, SEL selector, NSData* data, NSPasteboardType type) {
    assert(object == FaultBoard);
    ObservedData = data;
    return reinterpret_cast<BOOL (*)(id, SEL, NSData*, NSPasteboardType)>(OriginalSet)(object, selector, data, type);
}

void ReleaseThenThrow(id object, SEL selector) {
    assert(object == FaultBoard);
    reinterpret_cast<void (*)(id, SEL)>(OriginalRelease)(object, selector);
    @throw [NSException exceptionWithName:@"CSEAuthoredFailure" reason:@"after named cleanup submission" userInfo:nil];
}

void UncertainCleanupRetainsEverything() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        FaultBoard = [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:root.name.c_str()]];
        Method set = class_getInstanceMethod(object_getClass(FaultBoard), @selector(setData:forType:));
        Method release = class_getInstanceMethod(object_getClass(FaultBoard), @selector(releaseGlobally));
        OriginalSet = method_setImplementation(set, reinterpret_cast<IMP>(ObserveSet));
        const unsigned char units[] = {'Q', 0, 0, 0};
        assert(clipboard.Publish({false, {nullptr, 0}, 0, true, {units, 4}, 4, false, 0})
            == owner::Status::PrivateOnly);
        method_setImplementation(set, OriginalSet);
        OriginalRelease = method_setImplementation(release, reinterpret_cast<IMP>(ReleaseThenThrow));
        const auto result = clipboard.Close();
        method_setImplementation(release, OriginalRelease);
        assert(result == owner::Status::StateUncertain && clipboard.HoldsRoot());
        assert(root.queued == 0 && ObservedData != nil);
        // Retry is another oneway submission, not acknowledgement/readback.
        assert(clipboard.Close() == owner::Status::Ok);
        FaultBoard = nil;
    }
    std::cout << "framework_data_alive_after_cleanup_retry=" << (ObservedData != nil) << '\n';
    root.Drain();
}

void RejectedIdentityCleanup() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        Method unique = class_getClassMethod([NSPasteboard class], @selector(pasteboardWithUniqueName));
        Method name = class_getInstanceMethod([NSPasteboard class], @selector(name));
        Method release = class_getInstanceMethod([NSPasteboard class], @selector(releaseGlobally));
        OriginalUnique = method_setImplementation(unique, reinterpret_cast<IMP>(CaptureUnique));
        OriginalName = method_setImplementation(name, reinterpret_cast<IMP>(RejectedName));
        OriginalRelease = method_setImplementation(release, reinterpret_cast<IMP>(ObserveRejectedRelease));
        const auto result = owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard);
        assert(result == owner::Status::Unavailable);
        assert(clipboard.Close() == owner::Status::Ok);
        method_setImplementation(unique, OriginalUnique);
        method_setImplementation(name, OriginalName);
        method_setImplementation(release, OriginalRelease);
        // The fixture alone knows this is its captured real unique board. The
        // component was deliberately shown a rejected standard identity and
        // must NEVER submit global cleanup for that unadmitted board.
        assert(![FaultBoard.name isEqualToString:NSPasteboardNameGeneral]);
        [FaultBoard releaseGlobally];
        FaultBoard = nil;
    }
    root.Drain();
    if (RejectedReleases != 0) {
        std::cerr << "RED: global cleanup was attempted for rejected board identity\n";
        std::_Exit(78);
    }
}

NSInteger CountThenThrow(id object, SEL selector) {
    assert(object == FaultBoard);
    if (++CountCalls == 2) {
        @throw [NSException exceptionWithName:@"CSEAuthoredFailure" reason:@"after copied read" userInfo:nil];
    }
    return reinterpret_cast<NSInteger (*)(id, SEL)>(OriginalCount)(object, selector);
}

void ReadException() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        FaultBoard = [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:root.name.c_str()]];
        const unsigned char raw8[] = {'Q', 0};
        const unsigned char raw16[] = {'Q', 0, 0, 0};
        assert(clipboard.Publish({true, {raw8, 2}, 101, true, {raw16, 4}, 4, true, 2}) == owner::Status::Ok);
        Method method = class_getInstanceMethod(object_getClass(FaultBoard), @selector(changeCount));
        OriginalCount = method_setImplementation(method, reinterpret_cast<IMP>(CountThenThrow));
        owner::Snapshot snapshot{};
        snapshot.ticket = 99;
        const auto result = clipboard.Acquire(snapshot);
        method_setImplementation(method, OriginalCount);
        assert(result == owner::Status::Unavailable && snapshot.ticket == 99);
        if (clipboard.Close() != owner::Status::Ok) {
            // RED child only: the lost private read token cannot be recovered.
            // Submit cleanup for this proven unique board, then terminate the
            // owned process. Do not pretend the failed owner lease was retired.
            [FaultBoard releaseGlobally];
            std::cerr << "RED: exception lost the read token; owner cannot close\n";
            std::_Exit(79);
        }
        FaultBoard = nil;
    }
    root.Drain();
}

void RepeatedFactory() {
    Root root;
    unsigned int added = 0;
    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        const auto before = NothrowAllocations.load();
        const auto result = owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard);
        added = NothrowAllocations.load() - before;
        assert(result == owner::Status::Busy);
        assert(clipboard.Close() == owner::Status::Ok);
    }
    root.Drain();
    if (added != 0) {
        std::cerr << "RED: rejected factory allocated an untransferred backend: " << added << '\n';
        std::_Exit(77);
    }
}

void NamedRoundTripAndException() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        assert(!root.name.empty());
        std::cout << "named_board=" << root.name << '\n';
        NSPasteboard* board = [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:root.name.c_str()]];
        assert(![board.name isEqualToString:NSPasteboardNameGeneral]);
        const unsigned char raw8[] = {'A', 0, 'B', 0};
        const unsigned char units[] = {'A', 0, 0, 0, 'B', 0, 0x3d, 0xd8, 0, 0xde, 0, 0, 0xa5};
        owner::PreparedViews views{true, {raw8, sizeof(raw8)}, 101, true, {units, sizeof(units)}, 12, true, 10};
        assert(clipboard.Publish(views) == owner::Status::Ok);
        const unsigned char publicBytes[] = {'A', 0, 'B', 0xf0, 0x9f, 0x98, 0x80};
        NSData* actual = [board dataForType:NSPasteboardTypeString];
        assert(actual.length == sizeof(publicBytes));
        assert(std::memcmp(actual.bytes, publicBytes, sizeof(publicBytes)) == 0);
        NSString* publicText = [board stringForType:NSPasteboardTypeString];
        assert(publicText.length == 5 && [publicText characterAtIndex:1] == 0
            && [publicText characterAtIndex:3] == 0xd83d && [publicText characterAtIndex:4] == 0xde00);
        std::cout << "public_type=" << [NSPasteboardTypeString UTF8String] << '\n';
        owner::Snapshot snapshot{};
        assert(clipboard.Acquire(snapshot) == owner::Status::Ok);
        assert(snapshot.raw8.storageBytes == sizeof(raw8));
        assert(snapshot.unicode.unicodeStorage.storageBytes == sizeof(units));
        assert(std::memcmp(snapshot.unicode.unicodeStorage.data, units, sizeof(units)) == 0);
        NSData* bundle = [board dataForType:@"org.cse.clipboard.snapshot-v1"];
        NSMutableData* replacement = [bundle mutableCopy];
        auto* mutableBytes = static_cast<unsigned char*>(replacement.mutableBytes);
        mutableBytes[64] = 'Z';
        mutableBytes[68] = 'Z';
        const NSInteger before = board.changeCount;
        assert([board setData:replacement forType:@"org.cse.clipboard.snapshot-v1"]);
        std::cout << "same_owner_changeCount=" << before << ',' << board.changeCount << '\n';
        owner::Snapshot changed{};
        assert(clipboard.Acquire(changed) == owner::Status::Ok);
        assert(changed.raw8.data[0] == 'Z' && changed.unicode.unicodeStorage.data[0] == 'Z');
        assert(snapshot.raw8.data[0] == 'A');
        assert(clipboard.EndBorrow(changed) == owner::Status::Ok);
        [board clearContents];
        [board addTypes:@[NSPasteboardTypeString] owner:nil];
        assert([board setData:[NSData dataWithBytes:"external" length:8] forType:NSPasteboardTypeString]);
        owner::Snapshot external{};
        external.ticket = 99;
        assert(clipboard.Acquire(external) == owner::Status::MissingFormat && external.ticket == 99);
        assert(std::memcmp(snapshot.raw8.data, raw8, sizeof(raw8)) == 0);
        const unsigned char unpaired[] = {0, 0xd8, 0, 0};
        assert(clipboard.Publish({true, {raw8, sizeof(raw8)}, 101, true, {unpaired, 4}, 4, true, 2})
            == owner::Status::PrivateOnly);
        assert([board dataForType:NSPasteboardTypeString] == nil);
        owner::Snapshot lossless{};
        assert(clipboard.Acquire(lossless) == owner::Status::Ok);
        assert(std::memcmp(lossless.unicode.unicodeStorage.data, unpaired, sizeof(unpaired)) == 0);
        assert(clipboard.EndBorrow(lossless) == owner::Status::Ok);
        assert(clipboard.Close() == owner::Status::Busy);
        assert(clipboard.EndBorrow(snapshot) == owner::Status::Ok);
        assert(clipboard.Close() == owner::Status::Ok);
        board = nil;
    }
    root.Drain();

    @autoreleasepool {
        owner::Owner clipboard;
        assert(owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard) == owner::Status::Ok);
        FaultBoard = [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:root.name.c_str()]];
        Method method = class_getInstanceMethod(object_getClass(FaultBoard), @selector(setData:forType:));
        OriginalSet = method_setImplementation(method, reinterpret_cast<IMP>(SetThenThrow));
        const unsigned char raw8[] = {'Q', 0};
        const unsigned char raw16[] = {'Q', 0, 0, 0};
        owner::PreparedViews views{true, {raw8, 2}, 101, true, {raw16, 4}, 4, true, 2};
        const auto result = clipboard.Publish(views);
        method_setImplementation(method, OriginalSet);
        assert(result == owner::Status::StateUncertain && root.allocations == 0);
        assert(ObservedData != nil); // Staged copied data is intentionally backend-owned.
        assert(clipboard.Close() == owner::Status::Ok);
        FaultBoard = nil;
    }
    // The framework may retain its eagerly copied data after oneway submission.
    // Weak nil would not distinguish our local release from server/cache lifetime.
    std::cout << "framework_data_alive_after_close=" << (ObservedData != nil) << '\n';
    root.Drain();
}

void FactoryAndThreadFailures() {
    Root root;
    @autoreleasepool {
        owner::Owner clipboard;
        Method method = class_getClassMethod([NSPasteboard class], @selector(pasteboardWithUniqueName));
        IMP original = method_setImplementation(method, reinterpret_cast<IMP>(UniqueThenThrow));
        const auto result = owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard);
        method_setImplementation(method, original);
        assert(result == owner::Status::Unavailable && clipboard.HoldsRoot());
        assert(clipboard.Close() == owner::Status::Ok);
    }
    root.Drain();
    {
        owner::Owner clipboard;
        owner::Status result = owner::Status::Ok;
        std::thread worker([&] { result = owner::CreateNamedFixtureOwner(root.Dependencies(), clipboard); });
        worker.join();
        assert(result == owner::Status::WrongThread && clipboard.HoldsRoot());
        assert(clipboard.Close() == owner::Status::Ok);
    }
    root.Drain();
}
}

int main(int argc, char** argv) {
    Method general = class_getClassMethod([NSPasteboard class], @selector(generalPasteboard));
    Method named = class_getClassMethod([NSPasteboard class], @selector(pasteboardWithName:));
    IMP originalGeneral = method_setImplementation(general, reinterpret_cast<IMP>(ForbidGeneral));
    OriginalNamed = method_setImplementation(named, reinterpret_cast<IMP>(GuardNamed));
    if (argc == 2 && std::strcmp(argv[1], "read-exception") == 0) {
        ReadException();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "repeated-factory") == 0) {
        RepeatedFactory();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "rejected-identity") == 0) {
        RejectedIdentityCleanup();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "type-storage") == 0) {
        CopiedTypeStorage();
        return 0;
    }
    NamedRoundTripAndException();
    FactoryAndThreadFailures();
    ReadException();
    RepeatedFactory();
    RejectedIdentityCleanup();
    CopiedTypeStorage();
    UncertainCleanupRetainsEverything();
    assert(ForbiddenBoardCalls == 0);
    method_setImplementation(general, originalGeneral);
    method_setImplementation(named, OriginalNamed);
    std::cout << "named-only actual AppKit/exception/thread gates PASS; intercepted standard-board calls0\n";
}
