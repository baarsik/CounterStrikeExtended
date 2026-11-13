"""Inventory owning CMake target GL calls; lexical evidence, not runtime coverage."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT / 'tools'))
    from cmake_inventory import query_target
    evaluated = query_target(ROOT, 'hw_engine', configuration='Release', target_os='windows', architecture='x86')
    files = {}
    calls = []
    for item in evaluated['sources']:
        if item['language'] not in ('C', 'CXX'):
            continue
        path = Path(item['path'])
        if not path.is_file():
            continue
        data = path.read_bytes()
        text = data.decode('cp1252')
        matches = list(re.finditer(r'\b(qgl|gl)([A-Z]\w*)\s*\(', text))
        if not matches:
            continue
        relative = str(path.relative_to(ROOT))
        files[relative] = hashlib.sha256(data).hexdigest()
        for match in matches:
            calls.append({'file': relative, 'line': text.count('\n', 0, match.start()) + 1,
                          'operation': match[1] + match[2], 'raw_bypass': match[1] == 'gl'})
    cmake_inputs = sorted((ROOT / 'cmake').rglob('*.cmake')) + sorted((ROOT / 'cmake').rglob('CMakeLists.txt'))
    cmake_inputs += [ROOT / 'CMakeLists.txt', ROOT / 'tools/cmake_inventory.py']
    result = {'scope': 'Owning CMake Windows x86 Release source inventory; metadata and lexical calls, not runtime coverage',
              'cmake_input_hashes': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in cmake_inputs},
              'selected_source_hashes': files, 'calls': calls,
              'resource_budget': {'fragment_samplers': 3, 'vertex_samplers': 0, 'storage_resources': 0,
                  'uniform_blocks': 'Not implemented/reflected. Matrix+fog+environment constants fit <=4 per stage.',
                  'counts_basis': 'DetailTexture.cpp units0..2; installed platform world shader asset receipt in ART',
                  'qualified_SDL_source_profile': {'samplers_per_stage': 16, 'uniform_blocks_per_stage': 4,
                                                   'D3D12_fewer_storage_per_stage': 8},
                  'acceptance': 'Count compatibility only; offline shader translation/pipelines remain unqualified'}}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(calls)} selected lexical calls in {len(files)} source files')


if __name__ == '__main__':
    main()
