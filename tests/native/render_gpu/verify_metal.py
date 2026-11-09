"""Check offline Metal receipts and compiler-emitted AIR resource/layout metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def require(value, message):
    if not value:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(directory, source):
    receipt = json.loads((directory / 'compiler-receipt.json').read_text())
    pins_name = receipt.get('toolchain_pins', 'metal_6_2_windows.json')
    require(pins_name in ['metal_6_2_windows.json', 'metal_27b47_macos.json'], 'Unpinned Metal producer')
    pins = Path(__file__).with_name(pins_name)
    require(receipt['schema'] == 1 and receipt['stable_inputs_verified'], 'Unfinished Metal producer receipt')
    require(receipt['toolchain_pins_sha256'] == sha256(pins), 'Metal compiler/header pins differ')
    require(receipt['source_sha256'] == sha256(source), 'Metal shader source differs')
    require(sha256(directory / 'legacy_first.metal') == sha256(source), 'Staged Metal source differs')
    steps = receipt['steps']
    expected_labels = ['version']
    for configuration in ['Release', 'Debug']:
        expected_labels += [configuration + suffix for suffix in
                            ['-compile', '-reflection', '-LegacyVertex', '-LegacyFragment']]
    require([step['label'] for step in steps] == expected_labels, 'Incomplete compiler/linker steps')
    for step in steps:
        require(step['exit'] == 0, 'Failed Metal compiler step')
        require(step['log_sha256'] == sha256(directory / (step['label'] + '.log')), 'Metal diagnostic log differs')
        if step['label'].endswith(('-compile', '-reflection')):
            for flag in ['-std=metal3.1', '-mmacosx-version-min=14.0', '-fno-fast-math', '-ffp-contract=off',
                         '-fmetal-math-mode=safe', '-fmetal-math-fp32-functions=precise', '-Werror']:
                require(flag in step['command'], 'Missing shader policy: ' + flag)
    expected_outputs = {configuration + '/' + name for configuration in ['Release', 'Debug']
                        for name in ['legacy.air', 'legacy.ll', 'LegacyVertex.metallib', 'LegacyFragment.metallib']}
    require({row['path'] for row in receipt['outputs']} == expected_outputs and
            len(receipt['outputs']) == len(expected_outputs), 'Incomplete Metal outputs')
    for row in receipt['outputs']:
        path = directory / row['path']
        require(path.stat().st_size == row['bytes'] and sha256(path) == row['sha256'], 'Metal output differs')
    for configuration in ['Release', 'Debug']:
        for entry in ['LegacyVertex', 'LegacyFragment']:
            binary = directory / configuration / (entry + '.metallib')
            require(binary.read_bytes()[:4] == b'MTLB', 'Not a metallib container')
        text = (directory / configuration / 'legacy.ll').read_text()
        require('apple-macosx14.0.0"' in text, 'Wrong AIR deployment target')
        require('!"air.compile.fast_math_disable"' in text, 'Unsafe AIR math policy')
        nodes = dict(re.findall(r'^!(\d+) = !\{(.*)\}$', text, re.MULTILINE))
        buffers = [row for row in nodes.values() if '!"air.buffer"' in row]
        require(len(buffers) == 2, 'Unexpected buffer/storage resources')
        for name, size, fields in [
            ('VertexConstants', 320, [('ModelView', 0, 64), ('Projection', 64, 64), ('TextureMatrix', 128, 192)]),
            ('FragmentConstants', 144, [('Unit', 0, 48), ('Components', 48, 48), ('FogColor', 96, 16),
                                        ('Fog', 112, 16), ('FogAlpha', 128, 16)])]:
            rows = [row for row in buffers if '!"' + name + '"' in row]
            require(len(rows) == 1, 'Missing uniform block: ' + name)
            row = rows[0]
            require('!"air.buffer_size", i32 ' + str(size) + ',' in row and
                    '!"air.location_index", i32 0, i32 1' in row and '!"air.read"' in row,
                    'Incorrect uniform size/binding: ' + name)
            struct_node = re.search(r'!"air.struct_type_info", !(\d+)', row).group(1)
            variables = [(field, int(offset), int(size) * max(1, int(count)))
                         for offset, size, count, _, field in re.findall(
                             r'i32 (\d+), i32 (\d+), i32 (\d+), !"([^"]+)", !"([^"]+)"', nodes[struct_node])]
            require(variables == fields, 'Incorrect field offsets/strides: ' + name)
        for kind in ['texture', 'sampler', 'vertex_input']:
            rows = [row for row in nodes.values() if '!"air.' + kind + '"' in row]
            expected_count = 5 if kind == 'vertex_input' else 3
            locations = [int(re.search(r'!"air.location_index", i32 (\d+), i32 1', row).group(1)) for row in rows]
            require(sorted(locations) == list(range(expected_count)), 'Incorrect resource/attribute indices: ' + kind)
        for stage, entry in [('vertex', 'LegacyVertex'), ('fragment', 'LegacyFragment')]:
            node = re.search(r'^!air\.' + stage + r' = !\{!(\d+)\}$', text, re.MULTILINE)
            require(node is not None and '@' + entry + ',' in nodes[node.group(1)], 'Missing AIR shader entry')
        print(configuration, 'AIR uniform sizes=320/144 buffers=0 per stage textures/samplers=0..2 attributes=0..4')
    print('PASS offline Metal compiler/layout evidence; pixel and full-renderer acceptance are separate')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    verify(args.directory, args.source)
