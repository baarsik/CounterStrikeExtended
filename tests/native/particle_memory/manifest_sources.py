"""Evaluate the retained particle module manifest, including per-file settings."""

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
    import translate_msbuild as translator
    project = root / 'GoldSrc-SDK/projects/vs2026/particleman.vcxproj'
    evaluated = translator.evaluate(project, config)
    items = [item for item in evaluated['items'] if item['kind'] == 'ClCompile'
             and item['metadata'].get('ExcludedFromBuild', 'false') != 'true']
    lines = ['set(CSE_PARTICLE_SOURCES'] + ['  ' + quoted(item['path']) for item in items] + [')']
    includes = [value for value in evaluated['definitions']['ClCompile'].get(
        'AdditionalIncludeDirectories', '').split(';') if value]
    lines.extend(['set(CSE_PARTICLE_INCLUDE_DIRECTORIES ' + ' '.join(quoted(value) for value in includes) + ')'])
    for item in items:
        metadata = item['metadata']
        settings = {
            'INCLUDE_DIRECTORIES': metadata.get('AdditionalIncludeDirectories', '').split(';'),
            'COMPILE_DEFINITIONS': metadata.get('PreprocessorDefinitions', '').split(';')}
        if target_os != 'windows':
            settings['COMPILE_DEFINITIONS'] = [value for value in settings['COMPILE_DEFINITIONS']
                                               if value not in ('WIN32', '_WINDOWS')]
        forced = [value for value in metadata.get('ForcedIncludeFiles', '').split(';') if value]
        settings['COMPILE_OPTIONS'] = [('/FI' + value) for value in forced] if target_os == 'windows' else [
            part for value in forced for part in ('-include', value)]
        for key, values in settings.items():
            values = [value for value in values if value]
            if values:
                lines.append('set_property(SOURCE ' + quoted(item['path']) + ' APPEND PROPERTY ' + key + ' '
                             + ' '.join(quoted(value) for value in values) + ')')
    output.write_text('\n'.join(lines) + '\n')
    output.with_suffix('.json').write_text(json.dumps(evaluated, indent=2) + '\n')
