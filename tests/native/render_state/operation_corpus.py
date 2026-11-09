"""Inventory selected owning-manifest GL calls; lexical evidence, not runtime coverage."""
import argparse
import hashlib
import importlib.util
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
    spec = importlib.util.spec_from_file_location('cse_translate', ROOT / 'tools/translate_msbuild.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    manifest = ROOT / 'GoldSrc-SDK/projects/vs2026/hw_engine.vcxproj'
    evaluated = module.evaluate(manifest, 'Release')
    files = {}
    calls = []
    for item in evaluated['items']:
        if item['kind'] != 'ClCompile' or item['metadata'].get('ExcludedFromBuild', '').lower() == 'true':
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
    result = {'scope': 'Actual owning manifest Release source inventory; lexical calls, not runtime coverage',
              'manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
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
