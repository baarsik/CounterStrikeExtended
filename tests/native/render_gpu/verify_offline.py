"""Verify actual offline FXC containers, compiler receipts and reflected resource/layout output."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def verify(directory, source):
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    receipt = json.loads((directory / 'compiler-receipt.json').read_text(encoding='utf-8-sig'))
    assert receipt['source_sha256'] == sha(source)
    assert receipt['compiler_sha256'] == '3c1d8f3aa0d1c970732f14d61d3a4b5dfa8ca4b7c81d567643f39f6cef412010'
    assert len(receipt['entries']) == 4
    for record in receipt['entries']:
        entry = record['entry']
        binary = directory / record['configuration'] / (entry + '.dxbc')
        assembly = binary.with_name(entry + '.assembly.txt')
        assert record['exit'] == 0 and record['sha256'] == sha(binary)
        assert binary.read_bytes()[:4] == b'DXBC' and record['assembly_sha256'] == sha(assembly)
        text = assembly.read_text(encoding='utf-8-sig')
        table = text.split('// Resource Bindings:')[1].split('// Input signature:')[0]
        resources = []
        for line in table.splitlines():
            columns = line.removeprefix('//').split()
            if len(columns) >= 7 and columns[-1].isdigit():
                resources.append((columns[0], columns[1], columns[-2], int(columns[-1])))
        variables = [(match[0], int(match[1]), int(match[2])) for match in re.findall(
            r'//\s+(?:float4x4|float4)\s+(\w+)(?:\[\d+\])?;\s+// Offset:\s*(\d+) Size:\s*(\d+)', text)]
        if entry == 'LegacyVertex':
            assert record['profile'] == 'vs_5_1'
            assert resources == [('VertexConstants', 'cbuffer', 'cb0,space1', 1)]
            assert variables == [('ModelView', 0, 64), ('Projection', 64, 64), ('TextureMatrix', 128, 192)]
        else:
            assert entry == 'LegacyFragment' and record['profile'] == 'ps_5_1'
            expected = [(f'Sampler{i}', 'sampler', f's{i},space2', 1) for i in range(3)]
            expected += [(f'Texture{i}', 'texture', f't{i},space2', 1) for i in range(3)]
            expected += [('FragmentConstants', 'cbuffer', 'cb0,space3', 1)]
            assert resources == expected
            assert variables == [('Unit', 0, 48), ('Components', 48, 48), ('FogColor', 96, 16),
                                 ('Fog', 112, 16), ('FogAlpha', 128, 16)]
        print(record['configuration'], entry, 'verified', resources, variables)
    print('PASS actual offline FXC layout/resources; no renderer branch/pixel parity inference')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    verify(args.directory, args.source)
