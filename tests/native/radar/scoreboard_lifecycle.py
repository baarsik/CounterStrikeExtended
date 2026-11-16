"""Exercise the exact scoreboard discovery owner against a retiring VGUI tree."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(sys.argv[1])
compiler = sys.argv[2]
source = (root / 'src/cstrike/cl_dll/nextclient/hud/radar/integration/RadarHooks.cpp').read_text()


def extract(signature):
    start = source.index(signature)
    first = source.index('{', start)
    depth = 1
    end = first + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


fixture = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "hud/radar/integration/RadarScoreboard.h"
namespace vgui2 {
using VPANEL = std::uintptr_t;
using HPanel = unsigned;
struct IPanel {
    int queries = 0;
    int GetChildCount(VPANEL panel) {
        assert(panel != 0);
        ++queries;
        return panel == 1 ? 1 : 0;
    }
    VPANEL GetChild(VPANEL panel, int) {
        assert(panel == 1);
        return 2;
    }
    bool IsVisible(VPANEL panel) {
        return panel == 2;
    }
    const char* GetName(VPANEL panel) {
        return panel == 2 ? "ClientScoreBoard" : "root";
    }
};
struct ISurface {
    VPANEL root = 1;
    VPANEL GetEmbeddedPanel() {
        return root;
    }
};
struct IVGui {
    VPANEL resolved = 0;
    VPANEL HandleToPanel(HPanel) {
        return resolved;
    }
};
IPanel panels;
ISurface surfaceOwner;
IVGui vgui;
IPanel* ipanel() {
    return &panels;
}
ISurface* surface() {
    return &surfaceOwner;
}
IVGui* ivgui() {
    return &vgui;
}
}
'''
fixture += extract('struct ScoreboardUi') + ';\n' + extract('bool FindScoreboard(')
fixture += r'''
int main() {
    ScoreboardUi ui;
    assert(FindScoreboard(ui) && ui.panel == 2);
    // A still-live tracked scoreboard is restored even when it is hidden.
    vgui2::vgui.resolved = 3;
    ui = {};
    assert(FindScoreboard(ui, 42) && ui.panel == 3);
    // A retired tracked panel falls back to the active tree.
    vgui2::vgui.resolved = 0;
    ui = {};
    assert(FindScoreboard(ui, 42) && ui.panel == 2);
    // Shutdown retires the surface root before HUD_Shutdown restores placement.
    vgui2::surfaceOwner.root = 0;
    vgui2::panels.queries = 0;
    ui = {};
    assert(!FindScoreboard(ui, 42));
    assert(vgui2::panels.queries == 0 && ui.panel == 0);
    ui = {};
    assert(!FindScoreboard(ui));
    std::cout << "scoreboard live, tracked, recycled and retired-root discovery passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='cse-scoreboard-lifecycle-') as temporary:
    directory = Path(temporary)
    path = directory / 'fixture.cpp'
    path.write_text(fixture)
    radar = root / 'src/cstrike/cl_dll/nextclient'
    subprocess.run([compiler, '-std=c++17', '-UNDEBUG', '-fsanitize=address,undefined',
                    '-I' + str(radar), '-I' + str(radar / 'include'), str(path), '-o', str(directory / 'fixture')], check=True)
    subprocess.run([str(directory / 'fixture')], check=True)
