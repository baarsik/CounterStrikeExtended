"""Query the owning CMake target; never maintain a second source list."""

import json
from pathlib import Path
import sys


def sources(root, target_os, target, architecture='x86'):
    sys.path.insert(0, str(root / 'tools'))
    from cmake_inventory import query_target
    evaluated = {config: query_target(root, target, configuration=config, target_os=target_os,
                                     architecture=architecture) for config in ('Debug', 'Release')}
    selected = {}
    for config, value in evaluated.items():
        selected[config] = [item['path'] for item in value['sources'] if item['language'] in ('C', 'CXX')]
    if selected['Debug'] != selected['Release']:
        raise ValueError('Filesystem source membership differs between configurations')
    return selected['Release'], evaluated


if __name__ == '__main__':
    root = Path(sys.argv[1]).resolve()
    target = sys.argv[4] if len(sys.argv) > 4 else 'filesystem_stdio'
    architecture = sys.argv[5] if len(sys.argv) > 5 else 'x86'
    selected, evaluated = sources(root, sys.argv[2], target, architecture)
    output = Path(sys.argv[3])
    output.write_text('set(CSE_FILESYSTEM_SOURCES\n'
                      + ''.join(f'  "{Path(path).as_posix()}"\n' for path in selected) + ')\n')
    output.with_suffix('.json').write_text(json.dumps({'sources': selected, 'evaluated': evaluated}, indent=2) + '\n')
