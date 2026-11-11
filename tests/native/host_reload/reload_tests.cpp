#include "fixture.h"

namespace {
void Reset(const std::string &mode) {
    reset();
    events.clear();
    recentSave = mode == "absent" ? "" : "slot";
    headerValid = mode != "header";
    spawnSucceeds = mode != "spawn";
    allocations = 0;
    retirements = 0;
    entityCallbacks = 0;
    activations = 0;
    mapped = 0;
    markers = 0;
    headerReads = 0;
    giStateInfo = 0;
    cmd_wait = false;
    cmd_text.cursize = 0;
    cmd_text_untrusted.cursize = 0;
    cls = {};
    cls.state = ca_active;
    sv = {};
    sv.active = true;
    allocation = std::make_unique<edict_t[]>(EdictSlots);
    sv.edicts = allocation.get();
    sv.max_edicts = EdictSlots;
    sv.num_edicts = svs.maxclients + 1;
    if (mode != "missing" && mode != "absent" && mode != "spawn") {
        Bytes bytes = entry(260, "saved.HL1", {0x12, 0x34});
        if (mode == "archive") {
            const auto block = bytes;
            bytes.insert(bytes.end(), block.begin(), block.end());
            bytes.insert(bytes.end(), block.begin(), block.end());
        }
        files["SAVE/slot.sav"] = {bytes};
    }
    if (mode == "extraction") {
        writeBudget = 0;
    }
}

void Reload(const std::string &mode) {
    Reset(mode);
    const auto original = files.count("SAVE/slot.sav") ? files.at("SAVE/slot.sav").data : Bytes{};
    Cbuf_AddText("reload\nmarker\n");
    try {
        Cbuf_Execute();
    } catch (...) {
        std::cerr << "callbacks=" << entityCallbacks << " activations=" << activations
                  << " remaining_edicts=" << sv.max_edicts << '\n';
        throw;
    }
    check(markers == 1, "unrelated queued command lost or duplicated");
    check(giStateInfo == 0, "reload inherited explicit-load failure UI state");
    check(std::find(events.begin(), events.end(), "execute disconnect") == events.end(),
        "reload scheduled a teardown for its replacement server");
    if (mode == "success") {
        check(mapped == 1 && entityCallbacks == 0 && activations == 0, "successful load fell back");
        check(files.at("SAVE/saved.HL1").data == Bytes({0x12, 0x34}), "archive extraction changed");
    } else if (mode == "spawn") {
        check(entityCallbacks == 0 && activations == 0, "failed spawn still invokes game callbacks");
        check(sv.max_edicts == 0 && !allocation, "failed spawn leaked entity storage");
    } else {
        check(mapped == 0 && entityCallbacks == 1 && activations == 1, "failed load did not restart map");
        check(sv.active && sv.edicts == allocation.get() && sv.num_edicts == 3,
            "fallback entity lifetime did not survive nested game rules");
        check(allocations == 1 && retirements == 1, "fallback allocation lifetime mismatch");
    }
    if (!original.empty()) {
        check(files.at("SAVE/slot.sav").data == original, "input save bytes modified");
        check(files.at("SAVE/slot.sav").closed, "load leaked file handle");
    }
}

void ExplicitFailures() {
    for (const std::string mode : {"header", "archive", "extraction", "missing"}) {
        Reset(mode);
        Cbuf_AddText("marker\n");
        check(Host_Load("slot") == 0, "explicit failure became success");
        check(entityCallbacks == 0 && activations == 0 && mapped == 0, "explicit failure started a map");
        check(giStateInfo == (mode == "missing" ? 0 : STATE_ENDTRAINING), "explicit-load UI state changed");
        Cbuf_Execute();
        check(markers == 1, "explicit-load failure lost unrelated command");
        const auto disconnects = std::count(events.begin(), events.end(), "execute disconnect");
        check(disconnects == (mode == "missing" ? 0 : 1), "explicit-load disconnect behavior changed");
    }
}

void Cycles() {
    Reset("archive");
    constexpr int ReloadCycles = 4;
    for (int cycle = 0; cycle < ReloadCycles; ++cycle) {
        Cbuf_AddText("reload\nmarker\n");
        Cbuf_Execute();
        check(sv.active && sv.edicts == allocation.get() && sv.num_edicts == 3,
            "repeated reload lost live edicts");
    }
    check(allocations == ReloadCycles && retirements == ReloadCycles && activations == ReloadCycles,
        "repeated reload cleanup or activation count changed");
    check(markers == ReloadCycles && giStateInfo == 0, "repeated reload changed unrelated command/UI state");
}
} // namespace

int main(int argc, char **argv) {
    try {
        check(argc == 2, "expected a case name");
        const std::string mode = argv[1];
        if (mode == "explicit") {
            ExplicitFailures();
        } else if (mode == "cycles") {
            Cycles();
        } else {
            Reload(mode);
        }
        std::cout << "PASS " << mode << '\n';
        return 0;
    } catch (const std::exception &error) {
        for (const auto &event : events) {
            std::cerr << event << '\n';
        }
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
