#!/usr/bin/env python3
"""Run the actual targeted HFont reset body against live owning containers."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/vgui2/vgui_surfacelib/FontManager.cpp').read_text(encoding='latin1')
name = 'void CFontManager::ResetFont('
if name not in source:
    raise SystemExit('FAIL: missing targeted ResetFont owner')
start = source.index(name)
opening = source.index('{', start)
cursor = opening + 1
depth = 1
while depth:
    depth += (source[cursor] == '{') - (source[cursor] == '}')
    cursor += 1
body = source[start:cursor]
prefix = r'''
#include <cassert>
#include <climits>
#include <cstdint>
#include <map>
#include <set>
#include "utlvector.h"
namespace vgui2 { using HFont = std::uint32_t; }
struct Amalgam {
    int glyphSets = 1;
    void RemoveAll() { glyphSets = 0; }
};
class CFontManager {
public:
    CUtlVector<Amalgam> m_FontAmalgams;
    void ResetFont(vgui2::HFont);
};
'''
main = r'''
int main() {
    CFontManager manager;
    manager.m_FontAmalgams.AddMultipleToTail(4);
    manager.ResetFont(0);
    manager.ResetFont(UINT32_MAX);
    manager.ResetFont(4);
    assert(manager.m_FontAmalgams[0].glyphSets == 1);
    assert(manager.m_FontAmalgams[1].glyphSets == 1);
    manager.ResetFont(2);
    assert(manager.m_FontAmalgams[2].glyphSets == 1);
    assert(FontResetIntents()[&manager].count(2) == 1);
    assert(manager.m_FontAmalgams[1].glyphSets == 1);
    assert(manager.m_FontAmalgams[3].glyphSets == 1);
    assert(manager.m_FontAmalgams.Count() == 4);
    manager.ResetFont(2);
    assert(manager.m_FontAmalgams[2].glyphSets == 1);
    assert(FontResetIntents()[&manager].count(2) == 1);
}
'''
path = out / 'reset.cpp'
helper_start = source.index('std::map<const CFontManager*, std::set<vgui2::HFont>>& FontResetIntents()')
helper_open = source.index('{', helper_start)
helper_end = helper_open + 1
helper_depth = 1
while helper_depth:
    helper_depth += (source[helper_end] == '{') - (source[helper_end] == '}')
    helper_end += 1
path.write_text(prefix + source[helper_start:helper_end] + body + main)
executable = out / 'reset'
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                '-I', str(root / 'src/engine'), str(path), '-o', str(executable)], check=True)
subprocess.run([str(executable)], check=True)
print('PASS: targeted existing-HFont reset stages only its valid handle')
