#!/usr/bin/env python3
"""Exercise both actual renderer center-print owners without a game/session reset."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)

def function(source, name):
    match = re.search(r'^(?:void|bool) ' + name + r'\(', source, re.M)
    if not match:
        raise SystemExit('FAIL: missing owner function ' + name)
    start = source.index('{', match.start())
    depth = 1
    cursor = start + 1
    while depth:
        depth += (source[cursor] == '{') - (source[cursor] == '}')
        cursor += 1
    return source[match.start():cursor]

prefix = r'''
#include <cassert>
#include <cstring>
#include <string>
#include "localization_center_text.h"
#define Q_strncpy std::strncpy
char scr_centerstring[1024]{};
int scr_center_lines = 0;
int scr_erase_lines = 0;
float scr_centertime_start = 0;
float scr_centertime_off = 0;
struct { float value; } scr_centertime{2.0f};
struct { double time; bool intermission; } cl{8.0, false};
'''
main = r'''
int main() {
    char original[] = "Translated";
    SCR_CenterPrint(original);
    scr_centertime_off = 0.7f;
    const float start = scr_centertime_start;
    assert(!SCR_ReplaceCenterText("different", "replacement"));
    assert(std::strcmp(scr_centerstring, "Translated") == 0);
    assert(SCR_ReplaceCenterText("Translated", "New\nTranslation"));
    assert(scr_center_lines == 2 && scr_erase_lines >= 2);
    assert(scr_centertime_off == 0.7f && scr_centertime_start == start);
    char literal[] = "server literal";
    SCR_CenterPrint(literal);
    assert(!SCR_ReplaceCenterText("New\nTranslation", "wrong"));
    assert(std::strcmp(scr_centerstring, literal) == 0);
    scr_centertime_off = 0;
    assert(!SCR_ReplaceCenterText(literal, "expired"));
    cl.intermission = true;
    assert(SCR_ReplaceCenterText(literal, "intermission"));
    assert(scr_centertime_off == 0 && scr_centertime_start == start);
    std::string longText(1400, 'a');
    SCR_CenterPrint(&longText[0]);
    assert(SCR_ReplaceCenterText(longText.c_str(), "short"));
    assert(std::strcmp(scr_centerstring, "short") == 0);
    std::string utf8(1022, 'x');
    utf8 += "\xd0\x96";
    assert(SCR_ReplaceCenterText("short", utf8.c_str()));
    assert(std::strlen(scr_centerstring) == 1022);
    assert(!SCR_ReplaceCenterText(nullptr, "bad"));
    assert(!SCR_ReplaceCenterText(scr_centerstring, nullptr));
}
'''
for owner in ('screen.cpp', 'gl_screen.cpp'):
    source = (root / 'src/engine' / owner).read_text(encoding='latin1')
    text = prefix + function(source, 'SCR_CenterPrint') + '\n' + function(source, 'SCR_ReplaceCenterText') + main
    path = out / (owner + '.fixture.cpp')
    path.write_text(text)
    executable = out / owner
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root / 'src/engine'), str(path), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    print(owner, 'PASS')
