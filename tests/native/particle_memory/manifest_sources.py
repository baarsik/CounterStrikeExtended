"""Query the owning particle CMake target, including effective per-file settings."""

import json
from pathlib import Path
import sys


def quoted(value):
    return '"' + value.replace('\\', '/').replace('"', '\\"') + '"'


if __name__ == '__main__':
    root = Path(sys.argv[1]).resolve()
    target_os = sys.argv[2]
    config = sys.argv[3]
    output = Path(sys.argv[4])
    sys.path.insert(0, str(root / 'tools'))
    from cmake_inventory import query_target
    architecture = sys.argv[5] if len(sys.argv) > 5 else 'x86'
    evaluated = query_target(root, 'particleman', configuration=config, target_os=target_os,
                             architecture=architecture)
    items = [item for item in evaluated['sources'] if item['language'] in ('C', 'CXX')]
    if not items:
        raise ValueError('Owning particle CMake target selected no compiled sources')
    lines = ['set(CSE_PARTICLE_SOURCES'] + ['  ' + quoted(item['path']) for item in items] + [')']
    common_includes = set(items[0]['include_directories'])
    for item in items[1:]:
        common_includes.intersection_update(item['include_directories'])
    includes = [value for value in dict.fromkeys(items[0]['include_directories']) if value in common_includes]
    lines.extend(['set(CSE_PARTICLE_INCLUDE_DIRECTORIES ' + ' '.join(quoted(value) for value in includes) + ')'])
    for item in items:
        settings = {
            'INCLUDE_DIRECTORIES': item['include_directories'],
            'COMPILE_DEFINITIONS': item['compile_definitions'],
            'COMPILE_OPTIONS': item['compile_options']}
        for key, values in settings.items():
            values = [value for value in values if value]
            if values:
                lines.append('set_property(SOURCE ' + quoted(item['path']) + ' APPEND PROPERTY ' + key + ' '
                             + ' '.join(quoted(value) for value in values) + ')')
    output.write_text('\n'.join(lines) + '\n')
    output.with_suffix('.json').write_text(json.dumps(evaluated, indent=2) + '\n')
