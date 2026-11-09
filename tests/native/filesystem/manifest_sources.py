"""Consume the retained MSBuild evaluator; never maintain a second source list."""

import json
from pathlib import Path
import sys


def sources(root, target_os, project):
    sys.path.insert(0, str(root / 'tools'))
    import translate_msbuild as translator
    evaluated = {config: translator.evaluate(project, config) for config in ('Debug', 'Release')}
    selected = {}
    for config, value in evaluated.items():
        selected[config] = [item['path'] for item in value['items'] if item['kind'] == 'ClCompile'
                            and (item['metadata'].get('ExcludedFromBuild', 'false') != 'true'
                                 or item['metadata'].get('CseTargetOS') == target_os)]
    if selected['Debug'] != selected['Release']:
        raise ValueError('Filesystem source membership differs between configurations')
    return selected['Release'], evaluated


if __name__ == '__main__':
    root = Path(sys.argv[1]).resolve()
    project = Path(sys.argv[4]).resolve() if len(sys.argv) > 4 else root / 'GoldSrc-SDK/filesystem/filesystem_stdio.vcxproj'
    selected, evaluated = sources(root, sys.argv[2], project)
    output = Path(sys.argv[3])
    output.write_text('set(CSE_FILESYSTEM_SOURCES\n'
                      + ''.join(f'  "{Path(path).as_posix()}"\n' for path in selected) + ')\n')
    output.with_suffix('.json').write_text(json.dumps({'sources': selected, 'evaluated': evaluated}, indent=2) + '\n')
