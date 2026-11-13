"""Exercise actual bot path-search bodies against controlled blocked and visible traces."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / 'build/zbot-layout/crash/pathfind-probe'
OUTPUT.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


source = (ROOT / 'src/cstrike/dlls/bot/cs_bot_pathfind.cpp').read_text()
signatures = ('int CCSBot::FindOurPositionOnPath(', 'int CCSBot::FindPathPoint(')
(OUTPUT / 'pathfind.inc').write_text('\n\n'.join(function(source, signature) for signature in signatures))
executable = OUTPUT / 'pathfind_tests'
subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined',
                '-I' + str(ROOT / 'src/cstrike/dlls'), '-I' + str(OUTPUT),
                str(Path(__file__).with_name('pathfind_tests.cpp')), '-o', str(executable)], check=True)
subprocess.run([str(executable)], check=True)
