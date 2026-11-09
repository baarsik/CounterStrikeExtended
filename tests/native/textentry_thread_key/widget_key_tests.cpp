// Include the accepted complete real-owner fixture unchanged. Its imported
// constructors/destructors/target bodies and controlled typed services stay the
// same. This driver adds only key-service failure and explicit retirement gates.
#define main CseR6OriginalWidgetMain
#include "widget_tests.cpp"
#undef main

#include "os_api.h"

namespace clipboard = cse_textentry_clipboard;
namespace {
bool MainImageQuiescent(void*, const void* identity) {
    return identity == clipboard::NativePasteFrameIdentity() && liveKeys.empty() && textBlocks.empty();
}

clipboard::NativeFrameQuiescence Proof() {
    static int executableCodeAndContext = 0;
    return {sizeof(clipboard::NativeFrameQuiescence), 1, &executableCodeAndContext,
        clipboard::NativePasteFrameIdentity(), MainImageQuiescent};
}

int ResourceFailure(const char* name) {
    try {
        const KeyFault failure = std::strcmp(name, "key-create") == 0 ? KeyFault::CreateAgain
            : (std::strcmp(name, "key-set") == 0 ? KeyFault::SetNoMemory : KeyFault::LockBusy);
        {
            Panel parent(nullptr, "parent");
            ActualWidget widget(&parent, "A");
            fixtureActive = &widget;
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'B', 0};
            fixtureEvents.clear();
            SetKeyFault(failure);
            widget.Paste();
            SetKeyFault(KeyFault::None);
            Check(GetCells(widget).empty() && fixturePublications.empty(), "key failure has no text/publication");
            Check(!(widget.*Access(DirtyTag{})), "key failure leaves actual dirty state unchanged");
            Trace({"Count:A", "Count:A", "Get3:A"});
            fixtureEvents.clear();
            widget.Paste();
            Check(GetCells(widget) == Cells{L'A', L'B'}, "real widget recovers after resource failure");
            Trace({"Count:A", "Count:A", "Get3:A", "Undo:A", "Insert65:A", "Insert66:A",
                   "Action:A", "Focus:A"});
        }
        Check(liveKeys.empty() && textBlocks.empty(), "actual widget allocation/delivery ownership balanced");
        Check(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::Retired,
            "key retirement after complete actual widget destruction");
        std::cout << "PASS actual-owner native key failure: " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
}

int main(int argc, char** argv) {
    if (argc == 2 && (std::strcmp(argv[1], "key-create") == 0 || std::strcmp(argv[1], "key-set") == 0
        || std::strcmp(argv[1], "key-lock") == 0)) {
        return ResourceFailure(argv[1]);
    }
    const int result = CseR6OriginalWidgetMain(argc, argv);
    if (result != 0) {
        return result;
    }
    // The complete widget call and automatic objects have returned/destructed.
    // Code is in this permanently resident main executable; this is not a DLL
    // quiescence proof. Genuine DLL ownership is tested by modules_test.cpp.
    Check(clipboard::RetireNativePasteFrames(Proof()) == clipboard::NativeFrameRetireResult::Retired,
        "retire after actual R6 widget case");
    const auto stats = ReadKeyOsStats();
    Check(!stats.keyOwned && stats.nonnullDestructors == 0, "key released without exit callback");
    return 0;
}
